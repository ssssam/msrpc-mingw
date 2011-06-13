/* FIXME: add copyright */

#include <windows.h>
#include <rpc.h>

#include <stdarg.h>

typedef void (*RpcLogFunction) (unsigned int  status_code,
                                const char   *format,
                                va_list       args);

void rpc_default_log_function  (unsigned int status_code, const char *format, va_list args);
void rpc_set_log_function      (RpcLogFunction _log_function);

void rpc_log_error             (const char *format, ...);
void rpc_log_error_with_status (DWORD status, const char *format, ...);
void rpc_log_error_from_status (DWORD status);

typedef enum {
	RPC_SYSTEM_WIDE = 0,
	RPC_PER_USER = 1
} RpcFlags;

void rpc_init                  ();

int  rpc_server_start          (RPC_IF_HANDLE interface_spec, const char *endpoint_name, RpcFlags flags);
void rpc_server_stop           ();

int  rpc_client_bind           (handle_t *interface_handle, const char *endpoint_name, RpcFlags flags);
void rpc_client_unbind         (handle_t *interface_handle);

const char *rpc_get_active_endpoint_name ();

typedef RPC_ASYNC_STATE RpcAsyncCall;

/* Client-side API */
void rpc_async_call_init         (RpcAsyncCall *call);
void rpc_async_call_complete     (RpcAsyncCall *call, void *return_value);
int  rpc_async_call_complete_int (RpcAsyncCall *call);
int  rpc_async_call_cancel       (RpcAsyncCall *call);

/* Server-side API */
void rpc_async_call_return       (RpcAsyncCall *call, void *result);
int  rpc_async_call_is_cancelled (RpcAsyncCall *call);
