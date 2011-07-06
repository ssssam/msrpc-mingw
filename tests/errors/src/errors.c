/* MSRPC from mingw example - client. */

/* References:
 *   http://msdn.microsoft.com/en-us/library/aa379010%28v=VS.85%29.aspx
 */

#include "msrpc-mingw.h"
#include "hello.h"

#include <assert.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>

void *__RPC_USER MIDL_user_allocate (size_t size) {
	return malloc (size);
}

void __RPC_USER MIDL_user_free (void *data) {
	free (data);
}


int log_counter;

void test_log_cb (unsigned int  status_code,
                  const char   *format,
                  va_list       args) {
	assert (strcmp (format, "test") == 0);

	log_counter ++;
}

void test_log_modes () {
	log_counter = 0;

	rpc_set_log_function (test_log_cb);
	rpc_log_error ("test");
	assert (log_counter == 1);

	rpc_set_log_function (NULL);
	rpc_log_error ("test");
	assert (log_counter == 1);
}


void test_local_exception () {
	DWORD status = 0;

	RPC_TRY_EXCEPT {
		say_hello ("Client");
	}
	RPC_EXCEPT {
		status = RPC_GET_EXCEPTION_CODE();
	}
	RPC_END_EXCEPT

	assert (status == RPC_S_SERVER_UNAVAILABLE);
}


LPTOP_LEVEL_EXCEPTION_FILTER super_filter;

int exception_status;
jmp_buf exception_jmp_buf;

static LONG WINAPI exception_filter_cb (LPEXCEPTION_POINTERS exception_pointers) {
	LPEXCEPTION_RECORD exception;

	exception = exception_pointers->ExceptionRecord;
	exception_status = exception->ExceptionCode;

	if (exception_status == RPC_S_INVALID_NAF_ID) {
		longjmp (exception_jmp_buf, TRUE);
	}

	return super_filter (exception_pointers);
}

void test_exceptions_no_interference () {
	/* Our exception filter was already installed */
	exception_status = 0;

	if (setjmp (exception_jmp_buf) == 0)
		RaiseException (RPC_S_INVALID_NAF_ID, 0, 0, NULL);
	else
		assert (exception_status == RPC_S_INVALID_NAF_ID);
}


void test_local_exception_mt (int value) {
	/* Same as above, but vary the exception code to make sure the correct callback
	 * is called
	 */
	DWORD status = 0;
	DWORD expected_status = value;

	RPC_TRY_EXCEPT {
		RaiseException (expected_status, 0, 0, NULL);
	}
	RPC_EXCEPT {
		status = RPC_GET_EXCEPTION_CODE();
	}
	RPC_END_EXCEPT

	assert (expected_status == status);
}


void test_multithreaded () {
	const int N = 50;
	int i;

	for (i=0; i<N; i++) {
		if (i % 1) {
			CreateThread (NULL,
			              0,
			              (LPTHREAD_START_ROUTINE) test_local_exception_mt,
			              (void *)(i*100+100),
			              0,
			              NULL);
		} else {
			CreateThread (NULL,
			              0,
			              (LPTHREAD_START_ROUTINE) test_exceptions_no_interference,
			              NULL,
			              0,
			              NULL);
		}
	}
}

int main(int   argc,
         char *argv[]) {
	/* Install an exception filter *before* msrpc-mingw's one, so we can check
	 * that it doesn't interfere with process exception handling
	 */
	super_filter = SetUnhandledExceptionFilter (exception_filter_cb);

	rpc_client_bind (&hello_IfHandle, "hello-world", RPC_SYSTEM_WIDE);

	test_log_modes ();
	test_local_exception ();
	test_exceptions_no_interference ();
	test_multithreaded ();

	rpc_client_unbind (&hello_IfHandle);

	return 0;
}
