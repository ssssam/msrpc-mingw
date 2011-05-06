/* MSRPC from mingw example - server. */

/* References:
 *   http://msdn.microsoft.com/en-us/library/aa379010%28v=VS.85%29.aspx
 */

#include "hello.h"

#include <stdio.h>

void *__RPC_USER MIDL_user_allocate (size_t size) {
	return malloc (size);
}

void __RPC_USER MIDL_user_free (void *user) {
	free (user);
}


void say_hello (const unsigned char *name) {
	printf ("Hello %s. Yours, the Server\n", name);
}

int main(int   argc,
         char *argv[]) {
	RPC_STATUS status;

	status = RpcServerUseProtseqEp ("ncacn_np",  /* named pipes protocol */
	                                RPC_C_LISTEN_MAX_CALLS_DEFAULT /* ignore for ncacn_np */,
	                                "\\pipe\\hello",
	                                NULL  /* FIXME: access control */);

	if (status) exit (status);

	status = RpcServerRegisterIf (hello_v1_0_s_ifspec, NULL, NULL);

	if (status) exit (status);

	printf ("Server: listening\n");

	status = RpcServerListen (1, RPC_C_LISTEN_MAX_CALLS_DEFAULT, FALSE);

	if (status) exit (status);
}
