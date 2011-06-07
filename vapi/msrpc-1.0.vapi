[CCode (cprefix = "rpc_", cheader_filename = "msrpc-mingw.h")]

namespace Rpc {

	/* GLib integration */

	[CCode (cname = "rpc_glib2_init")]
	public void init ();


	/* Generic logging API */

	[CCode (cname = "rpc_default_log_function")]
	public void default_log_function (string domain, int errorlevel, string format, va_list args);

	[CCode (cname = "rpc_log_error")]
	[PrintfLike]
	public void log_error (string format, ...);

	[CCode (cname = "rpc_log_error_from_status")]
	public void log_error_from_status (uint32 status);


	/* Client and server setup */

	[CCode (cname = "RPC_IF_HANDLE")]
	[SimpleType]
	public struct InterfaceHandle {
	}

	[CCode (cname = "RpcServerFlags")]
	public enum ServerFlags {
		PER_SYSTEM,
		PER_USER
	}

	[CCode (cname = "rpc_server_start")]
	public int server_start (InterfaceHandle interface_spec, string endpoint_name, ServerFlags flags);

	[CCode (cname = "rpc_server_stop")]
	public void server_stop ();


	[CCode (cname = "handle_t")]
	public struct BindingHandle {
	}

	[CCode (cname = "rpc_client_bind")]
	public int client_bind (ref BindingHandle binding_handle, string endpoint_name);

	[CCode (cname = "rpc_client_unbind")]
	public void client_unbind (ref BindingHandle binding_handle);


	/* Async calls */

	[CCode (cname = "RpcAsyncCall")]
	public struct AsyncCall {
		[CCode (cname = "rpc_async_call_init")]
		public AsyncCall ();

		[CCode (cname = "rpc_async_call_complete")]
		public void complete (void *data);

		[CCode (cname = "rpc_async_call_complete_int")]
		public bool complete_bool ();

		[CCode (cname = "rpc_async_call_complete_int")]
		public int complete_int ();

		[CCode (cname = "rpc_async_call_cancel")]
		public void cancel ();

		[CCode (cname = "rpc_async_call_return")]
		public void return (void *result);

		[CCode (cname = "rpc_async_call_is_cancelled")]
		public bool is_cancelled ();
	}
}
