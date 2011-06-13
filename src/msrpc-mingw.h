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

#include <stdarg.h>

/***********************************************************************
 * Logging
 */

typedef void (*RpcLogFunction) (unsigned int  status_code,
                                const char   *format,
                                va_list       args);

/* The default action on error is to write it to the console and call exit() */
void rpc_default_log_function  (unsigned int status_code, const char *format, va_list args);

void rpc_set_log_function      (RpcLogFunction _log_function);

/* Report errors. If you called a Windows API function that returns a BOOL for
 * success or failure you can then call GetLastError() to obtain a value for
 * 'status'. rpc_log_error_with_status() will output the following:
 *   <format>: <error message>
 */
void rpc_log_error             (const char *format, ...);
void rpc_log_error_with_status (DWORD status, const char *format, ...);
void rpc_log_error_from_status (DWORD status);


/***********************************************************************
 * Init and shutdown
 */

/* By default RPC endpoints on Windows are available to all users of the
 * system. If you are transferring user-specific data, especially if it
 * is sensitive, use RPC_PER_USER mode. This will prepend the ID of the
 * current logon session to the endpoint name to make it unique and set
 * up a security callback so that only requests coming from the same
 * session as the server will be permitted.
 */
typedef enum {
	RPC_SYSTEM_WIDE = 0,
	RPC_PER_USER = 1
} RpcFlags;

/* This function is called automatically by rpc_server_start() and rpc_client_bind() */
void rpc_init                  ();

/* Init/shutdown server: 'interface_spec' is provided by the MIDL-generated header */
int  rpc_server_start          (RPC_IF_HANDLE interface_spec, const char *endpoint_name, RpcFlags flags);
void rpc_server_stop           ();

/* Bind/unbind client: 'binding_handle' is provided in the MIDL-generated header */
int  rpc_client_bind           (handle_t *binding_handle, const char *endpoint_name, RpcFlags flags);
void rpc_client_unbind         (handle_t *binding_handle);

/* Returns active endpoint name, or NULL */
const char *rpc_get_active_endpoint_name ();


/***********************************************************************
 * Asynchronous calls
 */

typedef RPC_ASYNC_STATE RpcAsyncCall;

/* Client-side API. rpc_async_call_complete() and rpc_async_call_complete_int()
 * will block until the call has completed server-side. */
void rpc_async_call_init         (RpcAsyncCall *call);
void rpc_async_call_complete     (RpcAsyncCall *call, void *return_value);
int  rpc_async_call_complete_int (RpcAsyncCall *call);
int  rpc_async_call_cancel       (RpcAsyncCall *call);

/* Server-side API. */
void rpc_async_call_return       (RpcAsyncCall *call, void *result);
int  rpc_async_call_is_cancelled (RpcAsyncCall *call);
