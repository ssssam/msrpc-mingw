bin_PROGRAMS = src/client src/server

AM_CPPFLAGS = $(MSRPC_CFLAGS)

noinst_HEADERS = src/hello.h

src_client_SOURCES = src/client.c src/hello_c.c
src_client_LDADD = $(MSRPC_LIBS)

src_server_SOURCES = src/server.c src/hello_s.c
src_server_LDADD = $(MSRPC_LIBS)

BUILT_SOURCES = src/hello.h src/hello_s.c src/hello_c.c

DISTCLEANFILES = ${BUILT_SOURCES}

src/hello_s.c: src/hello.idl src/hello.acf
	${AM_V_GEN} ${MIDL_WRAPPER} -o src -m ${MIDL} $^

src/hello_c.c src/hello.h: src/hello_s.c
