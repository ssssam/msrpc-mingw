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

void async_query (PRPC_ASYNC_STATE  async_query_handle,
                  unsigned char    *name,
                  unsigned char   **message) {
	RPC_STATUS status;
	const char *our_message = "Where have you been?";

	*message = MIDL_user_allocate (strlen (our_message) + 1);
	strcpy (*message, our_message);

	printf ("Got async call from %s\n", name);

	status = RpcAsyncCompleteCall (async_query_handle, NULL);
	if (status) {
		msrpc_log_error_from_status (status);
		exit(1);
	}
}

void sync_query (unsigned char    *name,
                 unsigned char   **message) {
	const char *our_message = "Where have you been?";

	*message = MIDL_user_allocate (strlen (our_message) + 1);
	strcpy (*message, our_message);

	printf ("Got sync call from %s; return %x -> %x (%i)\n", name, (int)our_message, (int)*message, strlen(*message));
}

int main () {
	msrpc_server_start (AsyncRPC_v1_0_s_ifspec, DEFAULT_ENDPOINT);

	printf ("Server listening as %s\n", DEFAULT_ENDPOINT);

	Sleep (60 * 1000);

	printf ("Shutting down - timeout\n");

	msrpc_server_stop ();
}
