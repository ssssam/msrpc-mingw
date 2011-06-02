/* RPC tester */

#include <windows.h>
#include "msrpc-mingw.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

HANDLE h_server = NULL,
       h_client = NULL;
HANDLE h_server_output = NULL,
       h_client_output = NULL;

void cleanup () {
	if (h_server != NULL)
		TerminateProcess (h_server, 0);

	if (h_server_output != NULL)
		CloseHandle (h_server_output);

	if (h_client != NULL)
		TerminateProcess (h_client, 0);

	if (h_client_output != NULL)
		CloseHandle (h_client_output);
}

void error_handler (const char *domain,
                           int         errorlevel,
                           const char *format,
                           va_list     args) {
	vprintf (format, args);

	cleanup ();
	exit (1);
}


HANDLE exec (const char *file_path,
             HANDLE     *p_output_pipe) {
	STARTUPINFO         startup_info = { 0 };
	PROCESS_INFORMATION process_info = { 0 };
	SECURITY_ATTRIBUTES security;

	HANDLE              pipe_read_handle, pipe_write_handle;
	BOOL                success;

	security.nLength = sizeof (SECURITY_ATTRIBUTES);
	security.bInheritHandle = TRUE;
	security.lpSecurityDescriptor = NULL;

	success = CreatePipe (&pipe_read_handle, &pipe_write_handle, &security, 0);

	if (! success)
		rpc_log_error_from_status (GetLastError ());

	startup_info.cb = sizeof (STARTUPINFO);
	startup_info.dwFlags = STARTF_USESTDHANDLES;
	startup_info.hStdOutput = pipe_write_handle;

	*p_output_pipe = pipe_read_handle;

	success = CreateProcess (file_path,
	                         NULL,
	                         NULL,
	                         NULL,
	                         TRUE,
	                         0,
	                         NULL,
	                         NULL,
	                         &startup_info,
	                         &process_info);

	if (! success)
		rpc_log_error_from_status (GetLastError ());

	return process_info.hProcess;
}

int read_to_buffer (HANDLE   h_stream,
                    char   **buffer,
                    int      n_bytes) {
	BOOL success;
	DWORD n_read;

	memset (buffer, 0, n_bytes);

	success = ReadFile (h_stream, buffer, n_bytes - 1, &n_read, NULL);

	if (! success)
		rpc_log_error_from_status (GetLastError ());

	return n_read;
}

void assert_line (HANDLE      h_stream,
                  const char *starts_with) {
	char buffer[256];

	read_to_buffer (h_stream, (char **)&buffer, 256);

	if (strncmp (buffer, starts_with, strlen (starts_with)))
		rpc_log_error ("Unexpected output: %s", buffer);
}

void assert_last_line (HANDLE      h_stream,
                       const char *starts_with) {
	char  buffer[256];
	char *c = buffer,
	     *last_line_position = c;

	read_to_buffer (h_stream, (char **)&buffer, 256);

	while (1) {
		for (; *c != '\r' && *c != '\n' && *c != 0; c++);

		if (*c == 0 || *(c+1) == 0 || *(c+2) == 0)
			break;

		c ++;
		last_line_position = c;
	};

	if (strncmp (last_line_position, starts_with, strlen (starts_with)))
		rpc_log_error ("Unacceptable output: %s", buffer);
}

void dump (HANDLE h_stream) {
	char buffer[256];

	read_to_buffer (h_stream, (char **)&buffer, 256);
	printf ("%s", buffer);
}

int main (int   argc,
          char *argv[]) {
	DWORD result;

	if (argc < 2) {
		printf ("Usage:\n");
		printf ("\t%s server client\n", argv[0]);
		exit (255);
	}

	rpc_set_log_function (error_handler);

	/* Start server */
	h_server = exec (argv[1], &h_server_output);

	assert_line (h_server_output, "server: listening");

	/* Run client */
	h_client = exec (argv[2], &h_client_output);

	result = WaitForSingleObject (h_client, 5000);

	if (result == WAIT_TIMEOUT) {
		dump (h_client_output);
		rpc_log_error ("Waiting for client timed out");
	}

	if (result != WAIT_OBJECT_0)
		rpc_log_error_from_status (GetLastError ());

	h_client = NULL;

	assert_last_line (h_client_output, "client: success");

	cleanup ();

	return 0;
}
