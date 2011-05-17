#include "msrpc-glib2.h"

/* Memory management stubs
 * -----------------------
 * 
 * Helpfully MIDL-generated interfaces allow us to plug in the GLib
 * malloc() code in here and things should play nicely with GLib and
 * Vala.
 */

void *__RPC_USER MIDL_user_allocate (size_t size) {
	return g_malloc (size);
}

void __RPC_USER MIDL_user_free (void *data) {
	g_free (data);
}


/* Initialisation
 * --------------
 *
 * Sets up error logging using the GLib logger.
 *
 */

void rpc_glib2_init () {
	rpc_set_log_function (g_logv);
}
