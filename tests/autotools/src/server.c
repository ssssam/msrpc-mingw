/* MSRPC from mingw example - server. */

/* References:
 *   http://msdn.microsoft.com/en-us/library/aa379010%28v=VS.85%29.aspx
 */

#include "msrpc-mingw.h"
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
	rpc_server_start (hello_v1_0_s_ifspec, "hello-world");

	Sleep (60 * 1000);

	rpc_server_stop ();
}
