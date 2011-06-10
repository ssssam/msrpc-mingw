#include "msrpc-mingw.h"
#include "peruser.h"

#include <stdio.h>

void *__RPC_USER MIDL_user_allocate (size_t size) {
	return malloc (size);
}

void __RPC_USER MIDL_user_free (void *user) {
	free (user);
}

int main (int   argc,
          char *argv[]) {
	BOOL           success;
	RPC_STATUS     status;
	DWORD          length;
	unsigned char *string_binding = NULL;
	const char    *victim_user_name;
	SID           *victim_sid;
	SID_NAME_USE   victim_account_type;
	char          *endpoint_name;
	char          *message = NULL;

	if (argc != 2) {
		printf ("Usage: rogue-client VICTIM_USER_NAME\n");
		exit (255);
	}

	/*victim_user_name = argv[1];

	victim_sid = malloc (48);
	length = 48;
	success = LookupAccountName (NULL,
	                             victim_user_name,
	                             victim_sid,
	                             &length,
	                             NULL,
	                             0,
	                             &victim_account_type);

	if (! success)
		rpc_log_error_from_status (GetLastError ());*/

	printf ("got sid: %x\n", victim_sid);

	/* Manually try to bind to the given endpoint, so we can
	 * try to break in to another user's RPC server.
	 */

	return;

	rpc_init ();

	status = RpcStringBindingCompose (NULL, "ncalrpc", NULL, (LPSTR)endpoint_name,
	                                  NULL, &string_binding);

	if (status)
		rpc_log_error_from_status (status);

	status = RpcBindingFromStringBinding (string_binding, &per_user_rpc_interface_handle);

	if (status)
		rpc_log_error_from_status (status);

	RpcStringFree (&string_binding);

	/* This is actually done automatically for us */

	/*status = RpcBindingSetAuthInfo (*interface_handle,
	                                NULL,
	                                RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
	                                RPC_C_AUTHN_WINNT,
	                                NULL,
	                                0);

	if (status) {
		rpc_log_error_from_status (status);*/

	tell_me_a_secret (&message);

	printf ("client: got secret '%s'\n", message);

	MIDL_user_free (message);

	rpc_client_unbind (&per_user_rpc_interface_handle);

	printf ("client: success\n");
}
