/* FIXME: add copyright */

#include <windows.h>
#include <rpc.h>

#include "msrpc-mingw.h"

#include <stdarg.h>
#include <stdio.h>


/* Memory management stubs
 * -----------------------
 */

void *__RPC_USER MIDL_user_allocate (size_t size) {
	return malloc (size);
}

void __RPC_USER MIDL_user_free (void *user) {
	free (user);
}



/* Error handling
 * --------------
 *
 * By default, errors are printed to the console and then process exits. You
 * can set a custom handler using rpc_set_log_function(). If using GLib,
 * note that you may do the following:
 *
 *   rpc_set_log_function (g_logv);
 *
 */

#define RPC_LOG_LEVEL_ERROR   (1<<2)

typedef void (*RpcLogFunction) (const char *domain,
                                  int         errorlevel,
                                  const char *format,
                                  va_list     args);

static RpcLogFunction log_function = rpc_default_log_function;

static LPTOP_LEVEL_EXCEPTION_FILTER super_exception_handler;

void rpc_default_log_function (const char *domain,
                                 int         errorlevel,
                                 const char *format,
                                 va_list     args) {
	vprintf (format, args);
	exit (1);
}

void rpc_log_error (const char *format, ...) {
	va_list args;
	va_start (args, format);
	log_function ("msrpc", RPC_LOG_LEVEL_ERROR, format, args);
	va_end (args);
}

void rpc_log_error_from_status (DWORD status) {
	char buffer[256];
	FormatMessage (FORMAT_MESSAGE_FROM_SYSTEM, NULL, status, 0, (LPSTR)&buffer, 255, NULL);
	rpc_log_error (buffer);
}

static LONG WINAPI exception_handler (LPEXCEPTION_POINTERS exception_pointers) {
	LPEXCEPTION_RECORD exception = exception_pointers->ExceptionRecord;

	/* Filter for RPC errors - not perfect, but avoids too many false
	 * positives. See winerror.h for the actual codes. */
	if (exception->ExceptionCode & 0x1700)
		rpc_log_error_from_status (exception->ExceptionCode);

	return super_exception_handler (exception_pointers);
}


/* Init and shutdown
 * -----------------
 *
 * Any errors will result in the rpc_log function being called.
 */

static RPC_IF_HANDLE server_interface = NULL;

int rpc_server_start (RPC_IF_HANDLE  interface_spec,
                        const char    *endpoint_name) {
	RPC_STATUS status;

	status = RpcServerUseProtseqEp ("ncalrpc",  /* local RPC only */
	                                RPC_C_LISTEN_MAX_CALLS_DEFAULT,
	                                (LPSTR)endpoint_name,
	                                NULL  /* FIXME: access control */);

	if (status) {
		rpc_log_error_from_status (status);
		return status;
	}

	status = RpcServerRegisterIf (interface_spec, NULL, NULL);

	if (status) {
		rpc_log_error_from_status (status);
		return status;
	}

	status = RpcServerListen (1, RPC_C_LISTEN_MAX_CALLS_DEFAULT, TRUE);

	if (status) {
		rpc_log_error_from_status (status);
		return status;
	}

	return 0;
}

void rpc_server_stop () {
	if (server_interface == NULL)
		return;

	RpcMgmtStopServerListening (NULL);

	/* This function will block until all in-progress RPC calls have completed */
	RpcServerUnregisterIf (server_interface, NULL, TRUE);
}


int rpc_client_bind (handle_t   *interface_handle,
                       const char *endpoint_name) {
	RPC_STATUS status;
	unsigned char *string_binding = NULL;

	super_exception_handler = SetUnhandledExceptionFilter (exception_handler);

	status = RpcStringBindingCompose (NULL,
	                                  "ncalrpc" /* local RPC only */,
	                                  NULL,
	                                  (LPSTR)endpoint_name,
	                                  NULL,
	                                  &string_binding);

	if (status) {
		rpc_log_error_from_status (status);
		return status;
	}

	status = RpcBindingFromStringBinding (string_binding,
	                                      interface_handle);

	if (status) {
		rpc_log_error_from_status (status);
		return status;
	}

	RpcStringFree (&string_binding);

	return 0;
}

void rpc_client_unbind (handle_t *interface_handle) {
	RpcBindingFree (interface_handle);
}


/* Asynchronous calls
 * ------------------
 *
 * Any errors will result in the rpc_log function being called.
 */

void rpc_async_call_init (RpcAsyncCall *call) {
	RPC_STATUS status;

	status = RpcAsyncInitializeHandle (call, sizeof(RPC_ASYNC_STATE));

	if (status) {
		rpc_log_error_from_status (status);
		return;
	}

	call->UserInfo = NULL;
	call->NotificationType = RpcNotificationTypeEvent;

	call->u.hEvent = CreateEvent (NULL, FALSE, FALSE, NULL);

	if (call->u.hEvent == 0) {
		rpc_log_error_from_status (status);
		return;
	}
}

/* Wait for completion and return result, client side */
void *rpc_async_call_complete (RpcAsyncCall *call) {
	DWORD      result;
	RPC_STATUS status;
	LPVOID     return_value = NULL;

	result = WaitForSingleObject (call->u.hEvent, INFINITE);

	if (result != WAIT_OBJECT_0) {
		rpc_log_error_from_status (result);
		return NULL;
	}

	status = RpcAsyncCompleteCall (call, &return_value);

	if (status != RPC_S_OK) {
		rpc_log_error_from_status (status);
		return NULL;
	}

	CloseHandle (call->u.hEvent);

	return return_value;
}

int rpc_async_call_cancel (RpcAsyncCall *call) {
	RPC_STATUS status;

	/* Issue an abortive cancel - don't wait for server to respond */
	status = RpcAsyncCancelCall (call, TRUE);

	if (status != RPC_S_OK) {
		rpc_log_error_from_status (status);
		return FALSE;
	}

	status = RpcAsyncCompleteCall (call, NULL);

	if (status == RPC_S_CALL_CANCELLED)
		return TRUE;

	if (status != RPC_S_OK) {
		rpc_log_error_from_status (status);
		return FALSE;
	}

	return TRUE;
}

/* Return value from server side */
void rpc_async_call_return (RpcAsyncCall *call,
                              void           *return_value) {
	RPC_STATUS status;

	status = RpcAsyncCompleteCall (call, return_value);

	if (status != RPC_S_OK) {
		rpc_log_error_from_status (status);
		return;
	}
}

void rpc_async_call_abort (RpcAsyncCall *call,
                             int             reason) {
	RPC_STATUS status;

	status = RpcAsyncAbortCall (call, reason);

	printf ("async abort call %i\n", status);
	if (status != RPC_S_OK) {
		rpc_log_error_from_status (status);
		return;
	}
	printf ("done\n");
}

int rpc_async_call_is_cancelled (RpcAsyncCall *call) {
	RPC_STATUS status;

	status = RpcServerTestCancel (RpcAsyncGetCallHandle (call));

	if (status == RPC_S_OK)
		return TRUE;

	if (status == RPC_S_CALL_IN_PROGRESS)
		return FALSE;

	if (status == RPC_S_INVALID_ASYNC_HANDLE) {
		/* Assume that the user passed a correct handle of a call that
		 * has already been cancelled ...
		 */
		 return TRUE;
	}

	rpc_log_error_from_status (status);
	return FALSE;
}
