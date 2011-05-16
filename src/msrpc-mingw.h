/* FIXME: add copyright */

#include <windows.h>
#include <rpc.h>

#include <stdarg.h>

void rpc_default_log_function  (const char *domain, int errorlevel, const char *format, va_list args);

void rpc_log_error             (const char *format, ...);
void rpc_log_error_from_status (DWORD status);

int  rpc_server_start          (RPC_IF_HANDLE interface_spec, const char *endpoint_name);
void rpc_server_stop           ();

int  rpc_client_bind           (handle_t *interface_handle, const char *endpoint_name);
void rpc_client_unbind         (handle_t *interface_handle);


typedef RPC_ASYNC_STATE RpcAsyncCall;

/* Client-side API */
void  rpc_async_call_init     (RpcAsyncCall *call);
void *rpc_async_call_complete (RpcAsyncCall *call);
int   rpc_async_call_cancel   (RpcAsyncCall *call);

/* Server-side API */
void  rpc_async_call_return       (RpcAsyncCall *call, void *result);
int   rpc_async_call_is_cancelled (RpcAsyncCall *call);
