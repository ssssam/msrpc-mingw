using Rpc;
using ValaInterface;

static const string global_message = "Bore da";

private class MessageRequest: Object {
	Rpc.AsyncCall call;
	char **message;

	public MessageRequest (Rpc.AsyncCall _call, char **_message) {
		call = _call;
		message = _message;
	}

	/* This function will be run inside the GLib main loop, so it will
	 * be able to make Gtk+ calls etc.
	 */
	public bool main_loop_callback () {
		print ("Inside the mainloop callback\n");
		Thread.usleep (2 * 1000 * 1000);

		*message = global_message;

		print ("Got message: %s\n", (string)(*message));
		call.return (null);

		return false;
	}
}

public class ValaServer: Object {
	/* This class must be a singleton, because we are currently using
	 * an implicit binding handle - therefore global state.
	 */
	static ValaServer instance = null;

	static int counter = 0;

	public ValaServer ()
	{
		Rpc.server_start (ValaInterface.spec, "vala-test-interface");
	}

	~ValaServer ()
	{
		Rpc.server_stop ();
	}

	public static ValaServer get_instance ()
	{
		if (instance == null)
			instance = new ValaServer ();
		return instance;
	}

	[CCode (cname = "vala_interface_ping")]
	public static int ping (string message)
	{
		stdout.printf ("%s\n", message);
		return counter++;
	}

	[CCode (cname = "vala_interface_get_message")]
	public static void get_message (AsyncCall call, char **message)
	{
		var request = new MessageRequest (call, ((char **)message));

		Idle.add (request.main_loop_callback);
	}
}

public int main (string[] args) {
	Rpc.init ();

	var server = new ValaServer();

	var main_loop = new MainLoop();

	Timeout.add_seconds (10, (GLib.SourceFunc)main_loop.quit);

	main_loop.run ();

	return 0;
}
