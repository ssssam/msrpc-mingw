#include "msrpc-mingw.h"
#include "peruser.h"

#include <stdio.h>

void *__RPC_USER MIDL_user_allocate (size_t size) {
	return malloc (size);
}

void __RPC_USER MIDL_user_free (void *user) {
	free (user);
}

char *get_process_owner () {
	BOOL  success;
	DWORD token_user_length,
	      username_length,
	      domain_length;
	char *username_buffer = malloc (256),
	     *domain_buffer = malloc (256);

	HANDLE        h_token;
	TOKEN_USER   *token_user = malloc (64);
	SID_NAME_USE  owner_type;

	success = OpenProcessToken (GetCurrentProcess (), TOKEN_QUERY, &h_token);

	if (! success) {
		rpc_log_error_from_status (GetLastError ());
		return NULL;
	}

	token_user_length = 64;
	success = GetTokenInformation (h_token, TokenUser, token_user, token_user_length, &token_user_length);

	if (! success) {
		rpc_log_error_from_status (GetLastError ());
		return NULL;
	}

	username_length = 256;
	domain_length = 256;
	success = LookupAccountSid (NULL, token_user->User.Sid,
	                            username_buffer, &username_length,
	                            domain_buffer, &domain_length,
	                            &owner_type);

	if (! success) {
		rpc_log_error_from_status (GetLastError ());
		return NULL;
	}

	if (owner_type != SidTypeUser)
		printf ("Warning: process is not owned by a user\n");

	CloseHandle (h_token);

	LocalFree (domain_buffer);

	return username_buffer;
}

void tell_me_a_secret (char **message) {
	const char *secret = "When I was a child I ate balloons";

	printf ("RPC function!\n");

	*message = MIDL_user_allocate (strlen (secret) + 1);
	strcpy (*message, secret);

	printf ("Done, returing %s\n", *message);

	return;
}

int main (int argc, char **argv) {
	BOOL  success;
	char  user_name[256];
	DWORD length;

	rpc_server_start (PerUserRPC_v1_0_s_ifspec, DEFAULT_ENDPOINT, RPC_PER_USER);

	length = 255;
	success = GetUserName (user_name, &length);

	if (! success)
		rpc_log_error_from_status (GetLastError ());

	printf ("server: listening (as user '%s')\n", user_name);
	fflush (stdout);

	LocalFree (user_name);

	Sleep (10000 * 1000);

	rpc_server_stop ();

	return 0;
}
