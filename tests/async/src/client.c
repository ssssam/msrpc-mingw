/* MSRPC in mingw example - asynchronous calls */

#include "msrpc-mingw.h"
#include "async.h"

#include <stdio.h>

void *__RPC_USER MIDL_user_allocate (size_t size) {
	return malloc (size);
}

void __RPC_USER MIDL_user_free (void *user) {
	free (user);
}

int main () {
	MsrpcAsyncCall  async_call;
	RPC_STATUS      status;
	DWORD           wait_result;
	unsigned char  *message;

	msrpc_client_bind (&async_rpc_interface_handle, DEFAULT_ENDPOINT);

	printf ("Client has binding to %s\n", DEFAULT_ENDPOINT);

	message = NULL;
	sync_query ("Joe", &message);

	printf ("%x -> %s (%i)\n", (int)message, message, strlen(message));
	MIDL_user_free (message);

	message = NULL;

	msrpc_async_call_init (&async_call);

	async_query (&async_call, "Frank", &message);

	msrpc_async_call_complete (&async_call);

	printf ("%x -> %s (%i)\n", (int)message, message, strlen(message));
	MIDL_user_free (message);

	msrpc_client_unbind (&async_rpc_interface_handle);
}
