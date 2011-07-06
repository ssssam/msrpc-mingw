using Rpc;
using ValaInterface;

public class ValaClient: Object {
	public ValaClient () {
		Rpc.client_bind (ref ValaInterface.binding_handle,
		                 "vala-test-interface",
		                 Rpc.Flags.SYSTEM_WIDE);
	}

	~ValaClient () {
		Rpc.client_unbind (ref ValaInterface.binding_handle);
	}
}

public int main (string[] args) {
	var client = new ValaClient();

	for (int i=0; i<5; i++)
		print ("%i\n", ValaInterface.ping ("Ping"));

	Rpc.AsyncCall call = Rpc.AsyncCall ();
	char *message = null;

	ValaInterface.get_message (call, &message);

	call.complete (null);

	print ("Got message: %s\n", (string)message);
	print ("client: success\n");

	return 0;
}
