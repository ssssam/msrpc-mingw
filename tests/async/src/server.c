/* MSRPC in mingw example - asynchronous calls */

#include "msrpc-mingw.h"
#include "async.h"

#include <stdio.h>

void async_query (RpcAsyncCall  *async_call,
                  unsigned char   *name,
                  unsigned char  **message) {
	const char *our_message = "Where have you been?";

	*message = MIDL_user_allocate (strlen (our_message) + 1);
	strcpy (*message, our_message);

	printf ("Got async call from %s\n", name);

	rpc_async_call_return (async_call, NULL);
}

void sync_query (unsigned char    *name,
                 unsigned char   **message) {
	const char *our_message = "Where have you been?";

	*message = MIDL_user_allocate (strlen (our_message) + 1);
	strcpy (*message, our_message);

	printf ("Got sync call from %s; return %x -> %x (%i)\n", name, (int)our_message, (int)*message, strlen(*message));
}

void black_hole (RpcAsyncCall *async_call) {
	printf ("server: Black hole .. asleep\n");
	Sleep (5 * 1000);

	if (!rpc_async_call_is_cancelled (async_call))
		printf ("server: WARNING: black hole did not receive a cancel!\n");

	rpc_async_call_return (async_call, NULL);

	printf ("server: Black hole - returned\n");
}

int main () {
	rpc_server_start (AsyncRPC_v1_0_s_ifspec, DEFAULT_ENDPOINT);

	printf ("Server listening as %s\n", DEFAULT_ENDPOINT);

	Sleep (60 * 1000);

	printf ("Shutting down - timeout\n");

	rpc_server_stop ();
}
