/* MSRPC from mingw example - client. */

/* References:
 *   http://msdn.microsoft.com/en-us/library/aa379010%28v=VS.85%29.aspx
 */

#include "msrpc-mingw.h"
#include "hello.h"

#include <signal.h>
#include <stdio.h>

void *__RPC_USER MIDL_user_allocate (size_t size) {
	return malloc (size);
}

void __RPC_USER MIDL_user_free (void *user) {
	free (user);
}

int main(int   argc,
         char *argv[]) {
	msrpc_client_connect (&hello_IfHandle, "hello-world");

	say_hello ("Client");

	msrpc_client_disconnect (&hello_IfHandle);
}
