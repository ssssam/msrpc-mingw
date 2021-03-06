/*
 * Copyright (c) 2011, JANET(UK)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of JANET(UK) nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Author: Sam Thursfield <samthursfield@codethink.co.uk>
 *
 * Known bugs:
 *   * If a test program exits with failure by the time we have called
 *     ReadFile() on its pipe, the tester will block indefinitely. We
 *     need some sort of timer callback, since SetCommTimeouts() does
 *     not work here.
 */

#include <windows.h>
#include "msrpc-mingw.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_CHILDREN  3

int in_cleanup = FALSE;

HANDLE h_process[MAX_CHILDREN] = { NULL };
HANDLE h_pipe[MAX_CHILDREN]    = { NULL };

typedef enum {
	TEST_NONE = 0,
	TEST_STANDARD,
	TEST_MULTIUSER
} TestMode;

typedef struct {
	char *server, *client, *rogue_client;
	TestMode test_mode;
} Options;

const char *test_user_logon_failure_error_message =
	"tester: Unable to log on as test user. For per-user testing, you must create a\n"
	"user account with username 'test' and password 'test'.\n";

const char *test_user_missing_privilege_error_message = 
	"tester: Execution of a process under another user account failed due to missing\n"
	"privileges. The test user must manually be given the privilege to 'Replace\n"
	"a process level token' (SeAssignPrimaryToken) - to do this, run the local\n"
	"security settings editor 'SECPOL.MSC' and, in the Local Policies > User Rights\n"
	"Assigment section you will find 'Replace a process level token' and be able to\n"
	"assign the privilege to user 'test'.\n";

void usage () {
	printf ("Usage: rpctester [options] SERVER CLIENT [ROGUE-CLIENT]\n\n"
	        "\t--standard:  test that CLIENT can connect to SERVER (default)\n"
	        "\t--multiuser: test that two sessions can run SERVER simultaneously\n"
	        "\t             (requires that a user account exists with username 'test'\n"
	        "\t              and password 'test')\n"); 
}

int parse_options (int      argc,
                   char    *argv[],
                   Options *options) {
	int i;

	memset (options, 0, sizeof(Options));

	for (i=1; i<argc; i++) {
		if (strncmp ("--", argv[i], 2) == 0) {
			if (options->test_mode == TEST_NONE) {
				if (strcmp ("--standard", argv[i]) == 0)
					options->test_mode = TEST_STANDARD;
				else
				if (strcmp ("--multiuser", argv[i]) == 0)
					options->test_mode = TEST_MULTIUSER;
				else
					return 0;
			} else
				return 0;
		} else
		if (options->server == NULL)
			options->server = argv[i];
		else
		if (options->client == NULL)
			options->client = argv[i];
		else
		if (options->rogue_client == NULL)
			options->rogue_client = argv[i];
		else
			return 0;
	}

	if (options->test_mode == TEST_NONE)
		options->test_mode = TEST_STANDARD;

	if (options->server == NULL || options->client == NULL)
		return 0;

	return 1;
}

void cleanup () {
	BOOL  success;
	DWORD result;
	int   i;

	if (in_cleanup)
		return;
	else
		in_cleanup = TRUE;

	for (i = 0; i < MAX_CHILDREN; i++) {
		if (h_process[i] != NULL) {
			success = GetExitCodeProcess (h_process[i], &result);

			if (! success) {
				printf ("tester: Unable to get active status for child process %i: ", i);
				rpc_log_error_from_status (GetLastError ());
			}

			if (result != STILL_ACTIVE)
				continue;

			success = TerminateProcess (h_process[i], 0);

			if (! success) {
				printf ("tester: Unable to kill child process %i: ", i);
				rpc_log_error_from_status (GetLastError ());
			}
		}

		if (h_pipe[i] != NULL)
			CloseHandle (h_pipe[i]);
	}
}

HANDLE exec_with_args (const char *file_path,
                       const char *args,
                       HANDLE      h_user_token,
                       HANDLE     *p_output_pipe) {
	SECURITY_ATTRIBUTES pipe_security;
	SECURITY_ATTRIBUTES process_security;
	STARTUPINFO         startup_info = { 0 };
	PROCESS_INFORMATION process_info = { 0 };
	LPTSTR              command_line = NULL;

	HANDLE              pipe_read_handle, pipe_write_handle;
	BOOL                success;

	/* Create I/O pipe - must be inheritable so it can be used by the child
	 * process as stdout.
	 */
	pipe_security.nLength = sizeof (SECURITY_ATTRIBUTES);
	pipe_security.bInheritHandle = TRUE;
	pipe_security.lpSecurityDescriptor = NULL;

	success = CreatePipe (&pipe_read_handle, &pipe_write_handle, &pipe_security, 0);

	if (! success) {
		printf ("tester: Unable to create pipe: ");
		rpc_log_error_from_status (GetLastError ());
	}

	process_security.nLength = sizeof (SECURITY_ATTRIBUTES);
	process_security.bInheritHandle = TRUE;
	process_security.lpSecurityDescriptor = NULL;

	startup_info.cb = sizeof (STARTUPINFO);
	startup_info.dwFlags = STARTF_USESTDHANDLES;
	startup_info.hStdOutput = pipe_write_handle;

	*p_output_pipe = pipe_read_handle;

	if (args != NULL) {
		/* Instead of separate file name and args we must use a command-line,
		 * to avoid the C runtime getting a confusing argv[] that doesn't
		 * contain the program filename in argv[0].
		 */
		command_line = malloc (strlen (file_path) + strlen (args) + 2);
		strcpy (command_line, file_path);
		strcat (command_line, " ");
		strcat (command_line, args);

		file_path = NULL;
	}

	if (h_user_token == NULL) {
		success = CreateProcess (file_path, command_line,
		                         &process_security, &process_security,
		                         TRUE, 0,
		                         NULL, NULL,
		                         &startup_info,
		                         &process_info);
	} else {
		success = ImpersonateLoggedOnUser (h_user_token);

		if (! success) {
			printf ("Could not impersonate user: ");
			rpc_log_error_from_status (GetLastError ());
		}

		/* Must be 'DETACHED_PROCESS' to work on Vista: see
		 *   http://blogs.msdn.com/b/alejacma/archive/2011/01/12/process-created-with-createprocessasuser-won-t-initialize-properly.aspx
		 */
		success = CreateProcessAsUser (h_user_token,
		                               file_path, command_line,
		                               &process_security, &process_security,
		                               TRUE, DETACHED_PROCESS,
		                               NULL, NULL,
		                               &startup_info,
		                               &process_info);

		if (! success && GetLastError() == ERROR_PRIVILEGE_NOT_HELD)
			rpc_log_error (test_user_missing_privilege_error_message);
	}

	if (command_line != NULL)
		free (command_line);

	if (! success) {
		printf ("tester: Unable to execute process %s: ", file_path);
		rpc_log_error_from_status (GetLastError ());
	}

	if (h_user_token != NULL) {
		success = RevertToSelf ();

		if (! success)
			rpc_log_error_from_status (GetLastError ());
	}

	return process_info.hProcess;
}

HANDLE exec (const char *file_path,
             HANDLE      h_user_token,
             HANDLE     *p_output_pipe) {
	return exec_with_args (file_path, NULL, h_user_token, p_output_pipe);
}

int wait_process (int n, const char *process_name) {
	DWORD  result;
	BOOL   success;
	HANDLE handle;

	handle = h_process[n];

	result = WaitForSingleObject (handle, 5000);

	if (result == WAIT_TIMEOUT) {
		// Not an error, so we can see server output
		/*rpc_log_error ("tester: Waiting for %s timed out\n", process_name);*/
		return 0;
	}

	if (result != WAIT_OBJECT_0) {
		printf ("tester: Error waiting for %s: ", process_name);
		rpc_log_error_from_status (GetLastError ());
	}

	h_process[n] = NULL;

	success = GetExitCodeProcess (handle, &result);

	if (! success)
		rpc_log_error_from_status (GetLastError ());

	return result;
}

int read_to_buffer (HANDLE   h_stream,
                    char   **buffer,
                    int      n_bytes) {
	BOOL success;
	DWORD n_read;

	memset (buffer, 0, n_bytes);

	success = ReadFile (h_stream, buffer, n_bytes - 1, &n_read, NULL);

	if (! success) {
		printf ("tester: Error reading from pipe: ");
		rpc_log_error_from_status (GetLastError ());
	}

	return n_read;
}

void read_line (HANDLE   h_stream,
                char    *buffer,
                int      n_bytes) {
	BOOL  success;
	DWORD length;
	int   n;

	for (n=0; n<n_bytes - 1; n++) {
		success = ReadFile (h_stream, buffer + n, 1, &length, 0);

		if (! success) {
			rpc_log_error_from_status (GetLastError ());
			return;
		}

		if (buffer[n] == '\n')
			break;
	}

	buffer[n+1] = 0;
}

void assert_line (HANDLE      h_stream,
                  const char *starts_with) {
	char buffer[256];

	read_line (h_stream, buffer, 256);

	if (strncmp (buffer, starts_with, strlen (starts_with)))
		rpc_log_error ("tester: assert_line: '%s'", buffer);
}

void assert_last_line (HANDLE      h_stream,
                       const char *starts_with) {
	char  buffer[4096];
	char *c = buffer,
	     *last_line_position = c;

	read_to_buffer (h_stream, (char **)&buffer, 4096);

	while (1) {
		for (; *c != '\r' && *c != '\n' && *c != 0; c++);

		if (*c == 0 || *(c+1) == 0 || *(c+2) == 0)
			break;

		c ++;
		last_line_position = c;
	};

	if (strncmp (last_line_position, starts_with, strlen (starts_with)))
		rpc_log_error ("tester: assert_last_line: '%s'", buffer);
}

void dump (HANDLE h_stream) {
	char buffer[4096];

	read_to_buffer (h_stream, (char **)&buffer, 4095);
	printf ("%s", buffer);
}

/* Return token for test user with required privileges to execute
 * programs as that user. Note that for this to work, some system
 * configuration is required:
 *   - you must have a local user account with name "test" and
 *     password "test"
 *   - you must run "secpol.msc", and in the Local Policies >
 *     User Rights Assignment section, give your test user the right to
 *     "Replace a process level token"
 */
void get_test_user_token (HANDLE *p_token) {
	BOOL  success;
	DWORD result;

	success = LogonUser ("test",
	                     ".",
	                     "test",
	                     LOGON32_LOGON_INTERACTIVE,
	                     LOGON32_PROVIDER_DEFAULT,
	                     p_token);

	if (! success) {
		result = GetLastError ();

		if (result == ERROR_LOGON_FAILURE) {
			rpc_log_error (test_user_logon_failure_error_message);
		} else {
			printf ("tester: Unable to log on second user: ");
			rpc_log_error_from_status (result);
		}
	}

	DWORD length = sizeof (TOKEN_PRIVILEGES) + sizeof (LUID_AND_ATTRIBUTES) * 2;
	TOKEN_PRIVILEGES *new_privileges = malloc (length);

	LUID se_increase_quota_privilege,
	     se_take_ownership_privilege,
	     se_assign_primary_token_privilege;

	success = LookupPrivilegeValue (NULL,
	                                "SeIncreaseQuotaPrivilege",
	                                &se_increase_quota_privilege);

	if (! success)
		rpc_log_error_from_status (GetLastError ());

	success = LookupPrivilegeValue (NULL,
	                                "SeTakeOwnershipPrivilege",
	                                &se_take_ownership_privilege);

	if (! success)
		rpc_log_error_from_status (GetLastError ());

	success = LookupPrivilegeValue (NULL,
	                                "SeAssignPrimaryTokenPrivilege",
	                                &se_assign_primary_token_privilege);

	if (! success)
		rpc_log_error_from_status (GetLastError ());

	new_privileges->PrivilegeCount = 3;
	new_privileges->Privileges[0].Luid = se_increase_quota_privilege;
	new_privileges->Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	new_privileges->Privileges[1].Luid = se_take_ownership_privilege;
	new_privileges->Privileges[1].Attributes = SE_PRIVILEGE_ENABLED;
	new_privileges->Privileges[2].Luid = se_assign_primary_token_privilege;
	new_privileges->Privileges[2].Attributes = SE_PRIVILEGE_ENABLED;

	/* If SeAssignPrimaryToken isn't present in the token (which it
	 * won't be if it's not been enabled in secpol.msc) this function
	 * will succeed anyway and just not enable the privilege. Then
	 * CreateProcessAsUser() will fail because "A required privilege is
	 * not held by the client."
	 */
	success = AdjustTokenPrivileges (*p_token,
	                                 FALSE,
	                                 new_privileges,
	                                 length,
	                                 NULL,
	                                 NULL);

	if (! success)
		rpc_log_error_from_status (GetLastError ());

	free (new_privileges);
}

void test_standard (Options options) {
	DWORD result;
	int   exit_code;

	/* Start server */

	h_process[0] = exec (options.server, NULL, &h_pipe[0]);

	assert_line (h_pipe[0], "server: listening");

	/* Run client */

	h_process[1] = exec (options.client, NULL, &h_pipe[1]);

	exit_code = wait_process (1, "client");

	if (exit_code) {
		printf ("\nServer output:\n"); dump (h_pipe[0]);
		printf ("\nClient output:\n"); dump (h_pipe[1]);
		rpc_log_error ("Client exited with an error");
	}

	assert_last_line (h_pipe[1], "client: success");
}

void test_multiuser (Options options) {
	DWORD   result;
	int     exit_code;
	char    buffer[256];
	HANDLE  second_user_token;
	char   *second_user_endpoint_name;

	/* Start two servers as different users */

	get_test_user_token (&second_user_token);
	h_process[0] = exec (options.server, second_user_token, &h_pipe[0]);
	h_process[1] = exec (options.server, NULL, &h_pipe[1]);
	CloseHandle (second_user_token);

	assert_line (h_pipe[0], "server: listening");
	assert_line (h_pipe[1], "server: listening");

	read_line (h_pipe[0], buffer, 256);

	if (strncmp (buffer, "server: endpoint name: ", strlen ("server: endpoint name: ")))
		rpc_log_error ("Unexpected output from server: %s\n", buffer);

	buffer[strlen(buffer) - 2] = 0;
	second_user_endpoint_name = buffer + strlen ("server: endpoint name: ");


	/* Check normal client can contact server */

	h_process[2] = exec (options.client, NULL, &h_pipe[2]);

	exit_code = wait_process (2, "client");

	if (exit_code != 0) {
		dump (h_pipe[2]);
		rpc_log_error ("tester: Client enountered an error contacting own user's server\n");
	}

	/* Check rogue client cannot contact other user's server */

	if (! options.rogue_client)
		return;

	h_process[2] = exec_with_args (options.rogue_client,
	                               second_user_endpoint_name,
	                               NULL,
	                               &h_pipe[2]);

	exit_code = wait_process (2, "client");

	if (exit_code != RPC_S_CALL_FAILED) {
		dump (h_pipe[2]);
		rpc_log_error ("tester: Rogue client encountered an error %i trying to contact "
		               "other user's server (RPC_S_CALL_FAILED (%i) is expected "
		               "when the security callback denies access).\n",
		               exit_code, RPC_S_CALL_FAILED);
	}
}

int main (int   argc,
          char *argv[]) {
	Options options;

	if (parse_options (argc, argv, &options) == FALSE) {
		usage ();
		exit (255);
	};

	rpc_init ();

	switch (options.test_mode) {
		case TEST_STANDARD:
			test_standard (options);
			break;

		case TEST_MULTIUSER:
			test_multiuser (options);
			break;
	}

	cleanup ();

	return 0;
}
