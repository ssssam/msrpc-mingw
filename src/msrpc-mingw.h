/* FIXME: add copyright */

#include <windows.h>
#include <rpc.h>

#include <stdarg.h>

void        msrpc_default_log_function  (const char *domain, int errorlevel, const char *format, va_list args);

void        msrpc_log_error             (const char *format, ...);
void        msrpc_log_error_from_status (DWORD status);

int         msrpc_server_start          (RPC_IF_HANDLE interface_spec, const char *endpoint_name);
void        msrpc_server_stop           ();

int         msrpc_client_connect        (handle_t *interface_handle, const char *endpoint_name);
void        msrpc_client_disconnect     (handle_t *interface_handle);

