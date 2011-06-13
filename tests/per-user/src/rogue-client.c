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
	char          *endpoint_name;
	char          *message = NULL;

	if (argc < 2) {
		printf ("Usage: rogue-client ENDPOINT\n");
		exit (255);
	}

	/* Manually try to bind to the given endpoint, so we can
	 * try to break in to another user's RPC server.
	 */

	rpc_init ();

	printf ("client: binding to %s\n", argv[1]);

	status = RpcStringBindingCompose (NULL, "ncalrpc",
	                                  NULL, argv[1],
	                                  NULL, &string_binding);

	if (status)
		rpc_log_error_from_status (status);

	status = RpcBindingFromStringBinding (string_binding, &per_user_rpc_interface_handle);

	if (status)
		rpc_log_error_from_status (status);

	RpcStringFree (&string_binding);

	status = RpcBindingSetAuthInfo (per_user_rpc_interface_handle,
	                                NULL,
	                                RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
	                                RPC_C_AUTHN_WINNT,
	                                NULL,
	                                0);

	if (status)
		rpc_log_error_from_status (status);

	/* Check the server is listening */

	status = RpcMgmtIsServerListening (per_user_rpc_interface_handle);

	if (status)
		rpc_log_error_from_status (status);

	/* Make a call (on access denied, we will get an RPC_S_CALL_FAILED exception) */

	tell_me_a_secret (&message);

	printf ("client: got secret '%s'\n", message);

	MIDL_user_free (message);

	rpc_client_unbind (&per_user_rpc_interface_handle);

	printf ("client: success\n");
}
