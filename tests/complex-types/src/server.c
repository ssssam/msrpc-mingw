/* MSRPC in mingw example - complex parameter types */

#include "msrpc-mingw.h"
#include "complex-types.h"

#include <assert.h>
#include <stdio.h>

void *__RPC_USER MIDL_user_allocate (size_t size) {
	return malloc (size);
}

void __RPC_USER MIDL_user_free (void *user) {
	free (user);
}

void test_call (int    strings_length,
                char  *strings[],
                int    people_length,
                Person people[]) {
	int i;

	assert (strings_length == 2);
	assert (people_length == 3);

	fprintf (stderr, "strings: %s, %s\n", strings[0], strings[1]);

	for (i = 0; i < people_length; i++)
		fprintf (stderr, "people: %s, %i\n", people[i].name, people[i].age);
}

int main () {
	rpc_server_start (complex_types_v1_0_s_ifspec, DEFAULT_ENDPOINT, RPC_SYSTEM_WIDE);

	printf ("server: listening (as %s)\n", DEFAULT_ENDPOINT);
	fflush (stdout);

	Sleep (60 * 1000);

	printf ("Shutting down - timeout\n");

	rpc_server_stop ();
}
