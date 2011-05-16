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
	RPC_ASYNC_STATE async_state;
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

	status = RpcAsyncInitializeHandle (&async_state, sizeof(RPC_ASYNC_STATE));
	if (status) exit (status);

	async_state.UserInfo = NULL;
	async_state.NotificationType = RpcNotificationTypeEvent;

	async_state.u.hEvent = CreateEvent (NULL, FALSE, FALSE, NULL);
	if (async_state.u.hEvent == 0) exit (1);

	async_query (&async_state, "Frank", &message);

	wait_result = WaitForSingleObject (async_state.u.hEvent, INFINITE);

	if (wait_result != WAIT_OBJECT_0) {
		msrpc_log_error_from_status (wait_result);
		exit (2);
	}

	status = RpcAsyncCompleteCall (&async_state, NULL);
	if (status != RPC_S_OK) {
		msrpc_log_error_from_status (status);
		exit (3);
	}

	CloseHandle (async_state.u.hEvent);

	printf ("%x -> %s (%i)\n", (int)message, message, strlen(message));
	MIDL_user_free (message);

	msrpc_client_unbind (&async_rpc_interface_handle);
}
