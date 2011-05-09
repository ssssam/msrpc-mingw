APPNAME = 'msrpc-mingw'
bld = 'build'
top = '.'

def configure(conf):
	conf.check_tool ('gcc msrpc')

	# Hack because we are not using MSVC to compile
	conf.env.DEFINES = 'TARGET_IS_NT351_OR_WIN95_OR_LATER'

	conf.env.LIB_RPC = ['rpcrt4']

def build(bld):
	client = bld (features = 'c cprogram msrpc_client',
	              source = ['client.c'],
	              msrpc_interface = ['hello.idl', 'hello.acf'],
	              target = 'client',
	              includes = ['build'],
	              use = 'RPC')

	server = bld (features = 'c cprogram msrpc_server',
	              source = ['server.c'],
	              msrpc_interface = ['hello.idl', 'hello.acf'],
	              target = 'server',
	              includes = ['build'],
	              use = 'RPC')
