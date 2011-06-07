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

void __RPC_USER MIDL_user_free (void *data) {
	free (data);
}

int main(int   argc,
         char *argv[]) {
	rpc_client_bind (&hello_IfHandle, "hello-world", RPC_SYSTEM_WIDE);

	say_hello ("Client");

	rpc_client_unbind (&hello_IfHandle);
}
