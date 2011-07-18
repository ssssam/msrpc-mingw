/* MSRPC in mingw example - complex parameter types */

#include "msrpc-mingw.h"
#include "complex-types.h"

#include <stdio.h>

void *__RPC_USER MIDL_user_allocate (size_t size) {
	return malloc (size);
}

void __RPC_USER MIDL_user_free (void *user) {
	free (user);
}

int main () {
	int strings_length = 2;
	char *strings[2] = { "Slum", "Headfirst" };

	Person people[] = {
		"David", 36,
		"George", 40,
		"Andy", 40
	};

	rpc_client_bind (&complex_types_interface_handle, DEFAULT_ENDPOINT, RPC_SYSTEM_WIDE);

	printf ("Client has binding to %s\n", DEFAULT_ENDPOINT);

	test_call (strings_length, strings, 3, people);

	rpc_client_unbind (&complex_types_interface_handle);

	printf ("client: success\n");
}
