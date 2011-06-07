/* FIXME: add copyright */

#include <windows.h>
#include <rpc.h>

#include "msrpc-mingw.h"

#include <stdarg.h>
#include <stdio.h>

#define RPC_ENDPOINT_MAX_LENGTH  52

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

static RpcLogFunction log_function = rpc_default_log_function;

static LPTOP_LEVEL_EXCEPTION_FILTER super_exception_handler;

void rpc_default_log_function (const char *domain,
                               int         errorlevel,
                               const char *format,
                               va_list     args) {
	vprintf (format, args);
	exit (1);
}

void rpc_set_log_function (RpcLogFunction _log_function) {
	log_function = _log_function;
}

void rpc_log_error (const char *format, ...) {
	va_list args;
	va_start (args, format);
	log_function ("Microsoft RPC", RPC_LOG_LEVEL_ERROR, format, args);
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


/* Per-user utilities
 * ------------------
 */


static int get_per_user_endpoint_name (const char  *prefix,
                                       char        *output_buffer) {
	HANDLE           process_access_token;
	TOKEN_STATISTICS token_data;

	BOOL  success;
	DWORD length;

	if (strlen (prefix) + 6 >= RPC_ENDPOINT_MAX_LENGTH) {
		rpc_log_error ("Warning: endpoint name %s is too long to reliably make "
		               "unique per-user (maximum endpoint name is %i",
		               RPC_ENDPOINT_MAX_LENGTH);
		return -1;
	}

	success = OpenProcessToken (GetCurrentProcess(), TOKEN_QUERY, &process_access_token);

	if (! success) {
		rpc_log_error_from_status (GetLastError ());
		return GetLastError ();
	}

	success = GetTokenInformation (process_access_token,
	                               TokenStatistics,
	                               &token_data,
	                               sizeof (TOKEN_STATISTICS),
	                               &length);

	if (! success) {
		rpc_log_error_from_status (GetLastError ());
		return GetLastError ();
	}

	/* Prepend the login session identifier to the endpoint name */
	snprintf (output_buffer + 1,
	          RPC_ENDPOINT_MAX_LENGTH,
	          "%x.%lx@%s",
	          token_data.AuthenticationId.LowPart,
	          token_data.AuthenticationId.HighPart,
	          prefix);

	return 0;
}

RPC_STATUS per_user_security_cb (RPC_IF_HANDLE  interface_spec,
                                 void          *context) {
	/* None shall pass! */
	return RPC_S_ACCESS_DENIED;
}

/* Init and shutdown
 * -----------------
 *
 * Any errors will result in the rpc_log() function being called.
 */

static RPC_IF_HANDLE server_interface = NULL;

int rpc_server_start (RPC_IF_HANDLE  interface_spec,
                      const char    *endpoint_name,
                      RpcFlags       flags) {
	char        per_user_endpoint_name[RPC_ENDPOINT_MAX_LENGTH + 1];
	RPC_STATUS  status;

	if (flags & RPC_PER_USER) {
		status = get_per_user_endpoint_name (endpoint_name, per_user_endpoint_name);

		if (status != 0)
			return status;

		endpoint_name = per_user_endpoint_name;
	}

	/* No access control is used on the endpoint itself, even if the
	 * server is per-user, as recommended by MSDN:
	 *
	 *   "At best, this method is a waste of CPU resources. At worst, in
	 *    many environments it is possible to circumvent the security
	 *    descriptor"
	 *
	 *   -- http://msdn.microsoft.com/en-us/library/aa373774(v=VS.85).aspx
	 */
	status = RpcServerUseProtseqEp ("ncalrpc",  /* local RPC only */
	                                RPC_C_LISTEN_MAX_CALLS_DEFAULT,
	                                (LPSTR)endpoint_name,
	                                NULL);

	if (status) {
		rpc_log_error_from_status (status);
		return status;
	}

	if (flags & RPC_PER_USER)
		status = RpcServerRegisterIf2 (interface_spec,
		                                NULL, NULL, 0, 0, -1,
		                                per_user_security_cb);
	else
		RpcServerRegisterIf2 (interface_spec, NULL, NULL, 0, 0, -1, NULL);

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
	RPC_STATUS status;

	if (server_interface == NULL)
		return;

	status = RpcMgmtStopServerListening (NULL);

	if (status) {
		rpc_log_error_from_status (status);
		return;
	}

	/* This function will block until all in-progress RPC calls have completed */
	status = RpcServerUnregisterIf (server_interface, NULL, TRUE);

	if (status) {
		rpc_log_error_from_status (status);
		return;
	}
}


int rpc_client_bind (handle_t   *interface_handle,
                     const char *endpoint_name,
                     RpcFlags    flags) {
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
void rpc_async_call_complete (RpcAsyncCall *call, 
                              void         *return_value) {
	DWORD      result;
	RPC_STATUS status;

	result = WaitForSingleObject (call->u.hEvent, INFINITE);

	if (result != WAIT_OBJECT_0) {
		rpc_log_error_from_status (result);
		return;
	}

	status = RpcAsyncCompleteCall (call, return_value);

	if (status != RPC_S_OK) {
		rpc_log_error_from_status (status);
		return;
	}

	CloseHandle (call->u.hEvent);
}

int rpc_async_call_complete_int (RpcAsyncCall *call) {
	int result = 0;
	rpc_async_call_complete (call, &result);
	return result;
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
                            void         *return_value) {
	RPC_STATUS status;

	status = RpcAsyncCompleteCall (call, return_value);

	if (status != RPC_S_OK) {
		rpc_log_error_from_status (status);
		return;
	}
}

void rpc_async_call_abort (RpcAsyncCall *call,
                           int           reason) {
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
