using Rpc;
using ValaInterface;

public int counter = 0;

public int vala_interface_ping (string message)
{
	stdout.printf ("%s\n", message);
	return counter++;
}

/* @message must be a 'ref' and not an 'out' parameter, because Vala
 * only assigns to out parameters at the end of the function and we need
 * them to be set before the call to AsyncCall.return().
 */
public void vala_interface_async_request (AsyncCall call, ref string message)
{
	message = "Bore da";

	call.return (null);
}

public class ValaServer: Object {
	public ValaServer ()
	{
		Rpc.server_start (ValaInterface.spec, "vala-test-interface");
	}

	~ValaServer ()
	{
		Rpc.server_stop ();
	}
}

public int main (string[] args) {
	var server = new ValaServer();

	Thread.usleep (1000 * 1000 * 10);

	return 0;
}
