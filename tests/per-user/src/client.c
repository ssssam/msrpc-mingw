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
	char *message = NULL;

	rpc_client_bind (&per_user_rpc_interface_handle, DEFAULT_ENDPOINT, RPC_PER_USER);

	tell_me_a_secret (&message);

	printf ("client: got secret '%s'\n", message);

	MIDL_user_free (message);

	rpc_client_unbind (&per_user_rpc_interface_handle);

	printf ("client: success\n");
}
