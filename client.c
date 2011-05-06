/* MSRPC from mingw example - client. */

/* References:
 *   http://msdn.microsoft.com/en-us/library/aa379010%28v=VS.85%29.aspx
 */

#include "hello.h"

#include <signal.h>
#include <stdio.h>

void *__RPC_USER MIDL_user_allocate (size_t size) {
	return malloc (size);
}

void __RPC_USER MIDL_user_free (void *user) {
	free (user);
}

LPTOP_LEVEL_EXCEPTION_FILTER super_exception_filter;

LONG WINAPI rpc_exception_filter (LPEXCEPTION_POINTERS exception_pointers) {
	LPEXCEPTION_RECORD exception = exception_pointers->ExceptionRecord;

	switch (exception->ExceptionCode) {
		case RPC_S_SERVER_UNAVAILABLE:
			printf ("RPC EXCEPTION: Unable to connect to server.\n");

			exit (1);
		case RPC_S_UNKNOWN_IF:
			printf ("RPC EXCEPTION: Unknown interface.\n");
			exit (1);

		default: {
			LPTSTR buffer;
			FormatMessage (FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
			               NULL,
			               exception->ExceptionCode,
			               0,
			               (LPSTR)&buffer,
			               0,
			               NULL);
			printf ("Received exception %i: %s\n", exception->ExceptionCode, buffer);
			return super_exception_filter (exception_pointers);
		}
	}
}

int main(int   argc,
         char *argv[]) {
	RPC_STATUS status;
	unsigned char *string_binding = NULL;

	/*(void) signal(0x6BA, rpc_exception_handler);*/
	/* IMPORTANT: Make SURE that if this code is being run from a DLL,
	 * the DLL cannot be unloaded from memory. Best to just not use it
	 * from a DLL. See: http://msdn.microsoft.com/en-us/library/ms680634%28v=VS.85%29.aspx#1
	 */
	super_exception_filter = SetUnhandledExceptionFilter (rpc_exception_filter);

	status = RpcStringBindingCompose (NULL,
	                                  "ncalrpc" /* named pipes protocol */,
	                                  NULL,
	                                  "hello-world",
	                                  NULL,
	                                  &string_binding);

	if (status) exit (status);

	printf ("Server address is: %s\n", string_binding);

	status = RpcBindingFromStringBinding (string_binding,
	                                      &hello_IfHandle);

	if (status) exit (status);

	say_hello ("Client");

	RpcStringFree (&string_binding);
	RpcBindingFree (&hello_IfHandle);
}
