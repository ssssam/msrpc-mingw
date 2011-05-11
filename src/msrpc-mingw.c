/* FIXME: add copyright */

#include <windows.h>
#include <rpc.h>

#include "msrpc-mingw.h"

#include <stdarg.h>
#include <stdio.h>


/* Error handling
 * --------------
 *
 * By default, errors are printed to the console and then process exits. You
 * can set a custom handler using msrpc_set_log_function(). If using GLib,
 * note that you may do the following:
 *
 *   msrpc_set_log_function (g_logv);
 *
 */

#define MSRPC_LOG_LEVEL_ERROR   (1<<2)

typedef void (*MsrpcLogFunction) (const char *domain,
                                  int         errorlevel,
                                  const char *format,
                                  va_list     args);

static MsrpcLogFunction log_function = msrpc_default_log_function;

static LPTOP_LEVEL_EXCEPTION_FILTER super_exception_handler;

void msrpc_default_log_function (const char *domain,
                                 int         errorlevel,
                                 const char *format,
                                 va_list     args) {
	vprintf (format, args);
	exit (1);
}

static void log_error (const char *format, ...) {
	va_list args;
	va_start (args, format);
	log_function ("msrpc", MSRPC_LOG_LEVEL_ERROR, format, args);
	va_end (args);
}

static void log_error_from_status (DWORD status) {
	char buffer[256];
	FormatMessage (FORMAT_MESSAGE_FROM_SYSTEM, NULL, status, 0, (LPSTR)&buffer, 255, NULL);
	log_error (buffer);
}

static LONG WINAPI exception_handler (LPEXCEPTION_POINTERS exception_pointers) {
	LPEXCEPTION_RECORD exception = exception_pointers->ExceptionRecord;

	/* Filter for RPC errors - not perfect, but avoids too many false
	 * positives. See winerror.h for the actual codes. */
	if (exception->ExceptionCode & 0x1700)
		log_error_from_status (exception->ExceptionCode);

	return super_exception_handler (exception_pointers);
}


/* Init and shutdown
 * -----------------
 *
 * Any errors will result in the log function being called.
 */

static RPC_IF_HANDLE server_interface = NULL;

int msrpc_server_start (RPC_IF_HANDLE  interface_spec,
                        const char    *endpoint_name) {
	RPC_STATUS status;

	status = RpcServerUseProtseqEp ("ncalrpc",  /* local RPC */
	                                RPC_C_LISTEN_MAX_CALLS_DEFAULT,
	                                "hello-world",
	                                NULL  /* FIXME: access control */);

	if (status) {
		log_error_from_status (status);
		return status;
	}

	status = RpcServerRegisterIf (interface_spec, NULL, NULL);

	if (status) {
		log_error_from_status (status);
		return status;
	}

	status = RpcServerListen (1, RPC_C_LISTEN_MAX_CALLS_DEFAULT, TRUE);

	if (status) {
		log_error_from_status (status);
		return status;
	}

	return 0;
}

void msrpc_server_stop () {
	if (server_interface == NULL)
		return;

	RpcMgmtStopServerListening (NULL);

	/* This function will block until all in-progress RPC calls have completed */
	RpcServerUnregisterIf (server_interface, NULL, TRUE);
}


int msrpc_client_connect (handle_t   *interface_handle,
                          const char *endpoint_name) {
	RPC_STATUS status;
	unsigned char *string_binding = NULL;

	super_exception_handler = SetUnhandledExceptionFilter (exception_handler);

	status = RpcStringBindingCompose (NULL,
	                                  "ncalrpc" /* named pipes protocol */,
	                                  NULL,
	                                  (LPSTR)endpoint_name,
	                                  NULL,
	                                  &string_binding);

	if (status) {
		log_error_from_status (status);
		return status;
	}

	status = RpcBindingFromStringBinding (string_binding,
	                                      interface_handle);

	if (status) {
		log_error_from_status (status);
		return status;
	}

	RpcStringFree (&string_binding);

	return 0;
}

void msrpc_client_disconnect (handle_t *interface_handle) {
	RpcBindingFree (interface_handle);
}
