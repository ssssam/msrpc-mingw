/*
 * Copyright (c) 2011, JANET(UK)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of JANET(UK) nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Author: Sam Thursfield <samthursfield@codethink.co.uk>
 */

#include <windows.h>
#include <rpc.h>

#include "msrpc-mingw.h"

#include <stdarg.h>
#include <stdio.h>

#define RPC_ENDPOINT_MAX_LENGTH  52

/* Stored so we can give a more helpful message when we get
 * RPC_S_SERVER_UNAVAILABLE exceptions.
 */
static char *active_endpoint_name = NULL;

static RPC_IF_HANDLE server_interface = NULL;

/***********************************************************************
 * Logging
 *
 * All log functions are thread-safe.
 */

static CRITICAL_SECTION log_mutex = { 0 };
static RpcLogFunction   log_function = NULL;

void rpc_default_log_function (unsigned int  status_code,
                               const char   *format,
                               va_list       args) {
	vprintf (format, args);
	exit (status_code);
}

static void log_init () {
	InitializeCriticalSection (&log_mutex);

	log_function = rpc_default_log_function;
}

void rpc_set_log_function (RpcLogFunction _log_function) {
	EnterCriticalSection (&log_mutex);

	log_function = _log_function;

	LeaveCriticalSection (&log_mutex);
}

static void call_log_function_ul (unsigned int  status_code,
                                  const char   *format,
                                  va_list       args) {
	if (log_function != NULL)
		log_function (status_code, format, args);
}

void rpc_log_error (const char *format, ...) {
	EnterCriticalSection (&log_mutex);

	va_list args;
	va_start (args, format);
	call_log_function_ul (1, format, args);
	va_end (args);

	LeaveCriticalSection (&log_mutex);
}

void rpc_log_error_from_status (DWORD status) {
	char buffer[256];

	EnterCriticalSection (&log_mutex);

	FormatMessage (FORMAT_MESSAGE_FROM_SYSTEM, NULL, status, 0, (LPSTR)&buffer, 255, NULL);
	call_log_function_ul (status, buffer, NULL);

	LeaveCriticalSection (&log_mutex);
}

void rpc_log_error_with_status (DWORD       status,
                                const char *format, ...) {
	char buffer1[256], *buffer2;

	EnterCriticalSection (&log_mutex);

	FormatMessage (FORMAT_MESSAGE_FROM_SYSTEM, NULL, status, 0, (LPSTR)&buffer1, 255, NULL);

	buffer2 = malloc (strlen (format) + strlen (buffer1) + 3);
	strcpy (buffer2, format);
	strcat (buffer2, ": ");
	strcat (buffer2, buffer1);

	va_list args;
	va_start (args, format);
	call_log_function_ul (status, buffer2, args);
	va_end (args);

	free (buffer2);

	LeaveCriticalSection (&log_mutex);
}


/***********************************************************************
 * Exception handling
 */

static LONG WINAPI exception_handler (LPEXCEPTION_POINTERS exception_pointers);

static DWORD exception_handler_tls_index;
static int   exception_handler_enable_global;

static LPTOP_LEVEL_EXCEPTION_FILTER super_exception_handler;

static void exception_handler_init () {
	exception_handler_tls_index = TlsAlloc ();
	exception_handler_enable_global = TRUE;
	super_exception_handler = SetUnhandledExceptionFilter (exception_handler);
}

void rpc_set_global_exception_handler_enable (int enable) {
	/* Int access is atomic on Win32 */
	exception_handler_enable_global = enable;
}

void _rpc_set_thread_exception_closure (struct _RpcExceptionClosure *closure) {
	TlsSetValue (exception_handler_tls_index, closure);
}

static LONG WINAPI exception_handler (LPEXCEPTION_POINTERS exception_pointers) {
	struct _RpcExceptionClosure *closure;
	LPEXCEPTION_RECORD exception;
	DWORD status;

	exception = exception_pointers->ExceptionRecord;
	status = exception->ExceptionCode;

	closure = TlsGetValue (exception_handler_tls_index);

	if (closure != NULL) {
		/* Local exception handler - jump back into the action */
		closure->status = exception->ExceptionCode;
		longjmp (closure->return_location, TRUE);
	}

	if (exception_handler_enable_global == TRUE) {
		if (status == RPC_S_SERVER_UNAVAILABLE)
			rpc_log_error_with_status (status,
			                           "RPC server is unavailable on endpoint '%s'\n",
			                           active_endpoint_name);
		else

		if (status == ERROR_ACCESS_DENIED)
			rpc_log_error_with_status (status,
			                           "Access denied to RPC endpoint '%s'\n",
			                           active_endpoint_name);
		else

		/* Filter for RPC errors - not perfect, but avoids too many false
		 * positives. See winerror.h for the actual codes. */
		if (status & 0x1700)
			rpc_log_error_from_status (status);

		return EXCEPTION_CONTINUE_EXECUTION;
	}

	return super_exception_handler (exception_pointers);
}


/***********************************************************************
 * Per-user utilities
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

	/* Get current process logon identifier. Another way to do this is
	 * to use GetClientInfo().
	 */

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
	snprintf (output_buffer,
	          RPC_ENDPOINT_MAX_LENGTH,
	          "%x.%lx@%s",
	          token_data.AuthenticationId.LowPart,
	          token_data.AuthenticationId.HighPart,
	          prefix);

	CloseHandle (process_access_token);

	return 0;
}

static TOKEN_USER *get_process_user () {
	BOOL        success;
	DWORD       length = 0;
	HANDLE      h_token;
	TOKEN_USER *result;

	success = OpenProcessToken (GetCurrentProcess(), TOKEN_QUERY, &h_token);

	if (! success) {
		rpc_log_error_from_status (GetLastError ());
		return NULL;
	}

	GetTokenInformation (h_token, TokenUser, NULL, 0, &length);

	if (length <= 0) {
		if (! success)
			rpc_log_error_from_status (GetLastError ());
		else
			rpc_log_error ("GetTokenInformation() returned zero length information\n");
		return NULL;
	}

	result = HeapAlloc (GetProcessHeap(),
	                    HEAP_ZERO_MEMORY | HEAP_GENERATE_EXCEPTIONS,
	                    length);

	success = GetTokenInformation (h_token, TokenUser, result, length, &length);

	if (! success) {
		rpc_log_error_from_status (GetLastError ());
		return NULL;
	}

	CloseHandle (h_token);

	return result;
}

static __stdcall RPC_STATUS per_user_security_cb (RPC_IF_HANDLE  interface_spec,
                                                  void          *context) {
	RPC_STATUS        status;
	BOOL              authorized;
	HANDLE            h_client = context;
	RPC_AUTHZ_HANDLE  privs = NULL;
	unsigned long     authentication_level,
	                  authentication_service,
	                  authorization_service;
	TOKEN_USER       *client_user,
	                 *server_user;

	status = RpcBindingInqAuthClient (h_client,
	                                  &privs,
	                                  NULL,
	                                  &authentication_level,
	                                  &authentication_service,
	                                  &authorization_service);

	if (status) {
		rpc_log_error_from_status (status);
		return RPC_S_ACCESS_DENIED;
	}

	if (authentication_service != RPC_C_AUTHN_WINNT)
		/* Unknown authentication method */
		return RPC_S_ACCESS_DENIED;

	if (authentication_level < RPC_C_AUTHN_LEVEL_PKT_PRIVACY)
		/* Not secure enough - and it's impossible to get a lower level for
		 * ncalrpc anyway */
		 return RPC_S_ACCESS_DENIED;

	status = RpcImpersonateClient (h_client);

	if (status) {
		rpc_log_error_from_status (status);
		return RPC_S_ACCESS_DENIED;
	}

	client_user = get_process_user ();

	status = RpcRevertToSelf ();

	if (status) {
		rpc_log_error_from_status (status);
		return RPC_S_ACCESS_DENIED;
	}

	server_user = get_process_user ();

	if (client_user == NULL || server_user == NULL)
		return RPC_S_ACCESS_DENIED;

	authorized = EqualSid (client_user->User.Sid, server_user->User.Sid);

	HeapFree (GetProcessHeap (), 0, client_user);
	HeapFree (GetProcessHeap (), 0, server_user);

	if (authorized)
		return RPC_S_OK;
	else
		return RPC_S_ACCESS_DENIED;
}



/***********************************************************************
 * Init and shutdown
 */

/* Called automatically, don't need this unless for some reason you are
 * binding / listening manually.
 */
void rpc_init () {
	static int initialized = FALSE;

	if (! initialized) {
		log_init ();
		exception_handler_init ();

		initialized = TRUE;
	}
}

int rpc_server_start (RPC_IF_HANDLE  interface_spec,
                      const char    *endpoint_name,
                      RpcFlags       flags) {
	RPC_STATUS status;
	char       per_user_endpoint_name[RPC_ENDPOINT_MAX_LENGTH + 1];

	rpc_init ();

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

	if (flags & RPC_PER_USER) {
		status = RpcServerRegisterAuthInfo (NULL, RPC_C_AUTHN_WINNT, NULL, NULL);

		if (status) {
			rpc_log_error_from_status (status);
			return status;
		}

		status = RpcServerRegisterIf2 (interface_spec,
		                               NULL,
		                               NULL,
		                               /* RPC_IF_ALLOW_SECURE_ONLY - this is on
		                                * by default in XP SP2 and above anyway */
		                               0x0008,
		                               0, -1,
		                               (RPC_IF_CALLBACK_FN *)per_user_security_cb);
	} else
		status = RpcServerRegisterIf2 (interface_spec, NULL, NULL, 0, 0, -1, NULL);

	if (status) {
		rpc_log_error_from_status (status);
		return status;
	}

	status = RpcServerListen (1, RPC_C_LISTEN_MAX_CALLS_DEFAULT, TRUE);

	if (status) {
		rpc_log_error_from_status (status);
		return status;
	}

	active_endpoint_name = strdup (endpoint_name);

	return 0;
}

void rpc_server_stop () {
	RPC_STATUS status;

	if (server_interface == NULL)
		return;

	if (active_endpoint_name != NULL) {
		free (active_endpoint_name);
		active_endpoint_name = NULL;
	}

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


int rpc_client_bind (handle_t   *binding_handle,
                     const char *endpoint_name,
                     RpcFlags    flags) {
	RPC_STATUS       status;
	char             per_user_endpoint_name[RPC_ENDPOINT_MAX_LENGTH + 1];
	unsigned char   *string_binding = NULL;
	RPC_SECURITY_QOS qos;

	rpc_init ();

	if (flags & RPC_PER_USER) {
		status = get_per_user_endpoint_name (endpoint_name, per_user_endpoint_name);

		if (status != 0)
			return status;

		endpoint_name = per_user_endpoint_name;
	}

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
	                                      binding_handle);

	if (status) {
		rpc_log_error_from_status (status);
		return status;
	}

	RpcStringFree (&string_binding);

	if (flags & RPC_PER_USER) {
		/* "The ncalrpc protocol sequence supports only RPC_C_AUTHN_WINNT,
		 *   but does support mutual authentication; supply an SPN and
		 *   request mutual authentication through the SecurityQOS parameter
		 *   to achieve this."
		 *
		 *   -- http://msdn.microsoft.com/en-us/library/aa375608(v=vs.85).aspx
		 *
		 * Perhaps we can support this?
		 */

		qos.Version = 1;
		qos.Capabilities = RPC_C_QOS_CAPABILITIES_DEFAULT; /* MUTUAL_AUTH*/
		qos.IdentityTracking = RPC_C_QOS_IDENTITY_STATIC;
		qos.ImpersonationType = RPC_C_IMP_LEVEL_IMPERSONATE;

		/* Auth seems to be set for client even if we don't call this */
		status = RpcBindingSetAuthInfoEx (*binding_handle,
		                                  NULL,
		                                  RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
		                                  RPC_C_AUTHN_WINNT,
		                                  NULL,
		                                  0,
		                                  &qos);

		if (status) {
			rpc_log_error_from_status (status);
			return status;
		}
	}

	active_endpoint_name = strdup (endpoint_name);

	return 0;
}

void rpc_client_unbind (handle_t *binding_handle) {
	RpcBindingFree (binding_handle);

	if (active_endpoint_name != NULL) {
		free (active_endpoint_name);
		active_endpoint_name = NULL;
	}
}

const char *rpc_get_active_endpoint_name () {
	return active_endpoint_name;
}


/***********************************************************************
 * Asynchronous calls
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

	if (status != RPC_S_OK) {
		rpc_log_error_from_status (status);
		return;
	}
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
