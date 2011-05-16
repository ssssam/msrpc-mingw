/* MSRPC in mingw example - asynchronous calls */

#include "msrpc-mingw.h"
#include "async.h"

#include <stdio.h>

int main () {
	RpcAsyncCall   async_call;
	RPC_STATUS     status;
	DWORD          wait_result;
	unsigned char *message;
	int            cancelled;

	rpc_client_bind (&async_rpc_interface_handle, DEFAULT_ENDPOINT);

	printf ("Client has binding to %s\n", DEFAULT_ENDPOINT);

	message = NULL;
	sync_query ("Joe", &message);

	printf ("%x -> %s (%i)\n", (int)message, message, strlen(message));
	MIDL_user_free (message);

	message = NULL;

	rpc_async_call_init (&async_call);

	async_query (&async_call, "Frank", &message);

	rpc_async_call_complete (&async_call);

	printf ("%x -> %s (%i)\n", (int)message, message, strlen(message));
	MIDL_user_free (message);

	rpc_async_call_init (&async_call);
	black_hole (&async_call);

	printf ("client: Called black_hole ()\n");
	cancelled = rpc_async_call_cancel (&async_call);
	printf ("client: Cancel completed: %i\n", cancelled);

	rpc_client_unbind (&async_rpc_interface_handle);
}
