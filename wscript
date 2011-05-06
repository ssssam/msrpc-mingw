APPNAME = 'msrpc-mingw'
bld = 'build'
top = '.'

def configure(conf):
	conf.check_tool ('gcc')

	# Hack because we are not using MSVC to compile
	conf.env.DEFINES = 'TARGET_IS_NT351_OR_WIN95_OR_LATER'

	# custom import lib for rpcrt4 as mingw's is too old
	#conf.env.LIB_RPC_7 = ['rpcrt4-v7']
	#conf.env.LIBPATH_RPC_7 = ['..']
	conf.env.LIB_RPC = ['rpcrt4']

def build(bld):
	client = bld (features = 'c cprogram',
	              source = ['client.c', 'hello_c.c'],
	              target = 'client',
	              use = 'RPC')

	server = bld (features = 'c cprogram',
	              source = ['server.c', 'hello_s.c'],
	              target = 'server',
	              use = 'RPC')
