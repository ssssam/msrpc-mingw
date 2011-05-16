APPNAME = 'msrpc-mingw'
VERSION = '0.1.0'
bld = 'build'
top = '.'

def options(opt):
	opt.tool_options('gcc gnu_dirs msrpc')

def configure(conf):
	conf.check_tool ('gcc gnu_dirs msrpc')

	# We do this manually; real users of the waf plugin will include the
	# pkg-config file which defines all this.
	conf.env['DEFINES_RPC'] = 'TARGET_IS_NT50_OR_LATER'
	conf.env['LIBPATH_RPC'] = 'd:/codethink/w32api-3.17-2-mingw32/lib'
	conf.env['INCLUDES'] = 'd:/codethink/w32api-3.17-2-mingw32/include'
	conf.env['LIB_RPC'] = 'rpcrt4'

	# Prevent gcc4 crashes
	conf.env['LDFLAGS'] = '-Wl,--enable-auto-import'

def build(bld):
	bld (features     = 'c cstlib',
	     # FIXME: this work when called from outside base dir ..
	     source       = bld.path.ant_glob('src/*.c'),
	     target       = 'msrpc-mingw',
	     install_path = '${LIBDIR}')

	bld.install_files('${INCLUDEDIR}', 'src/msrpc-mingw.h')

	if bld.is_install:
		bld (rule="""sed -e 's#@prefix@#${PREFIX}#' -e 's#@libdir@#${LIBDIR}#' -e 's#@bindir@#${BINDIR}#' -e 's#@version@#%s#' < ${SRC} > ${TGT}""" % VERSION,
		     source='msrpc-mingw-1.0.pc.in',
		     target='msrpc-mingw-1.0.pc',
		     install_path='${LIBDIR}/pkgconfig')

	bld.install_files('${BINDIR}', 'bin/midl-wrapper')
	bld.install_files('${DATADIR}/aclocal', 'm4macros/msrpc-mingw-1.0.m4')

	bld.recurse ('tests')
