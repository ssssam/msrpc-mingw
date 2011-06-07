#include "msrpc-mingw.h"
#include "peruser.h"

#include <stdio.h>

void *__RPC_USER MIDL_user_allocate (size_t size) {
	return malloc (size);
}

void __RPC_USER MIDL_user_free (void *user) {
	free (user);
}

int main () {
	RpcAsyncCall   async_call;
	RPC_STATUS     status;
	DWORD          wait_result;
	unsigned char *message;
	int            counter = 0;
	int            cancelled;

	rpc_client_bind (&peruser_interface_handle, DEFAULT_ENDPOINT, RPC_PER_USER);

	rpc_client_unbind (&peruser_interface_handle);

	printf ("client: success\n");
}
