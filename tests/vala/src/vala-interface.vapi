/* FIXME: generate automatically */

using Rpc;

[CCode (cheader_filename = "vala.h")]

namespace ValaInterface {
	[CCode (cname = "vala_v1_0_s_ifspec")]
	public const InterfaceHandle spec;

	[CCode (cname = "vala_interface_handle")]
	public BindingHandle binding_handle;

	[CCode (cname = "vala_interface_ping")]
	public int ping (string message);

	[CCode (cname = "vala_interface_async_request")]
	public void async_request (Rpc.AsyncCall call, ref string message);
}
