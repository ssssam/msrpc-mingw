APPNAME = 'msrpc-mingw'
VERSION = '0.1.0'
bld = 'build'
top = '.'

def options(opt):
	opt.tool_options('gcc gnu_dirs')

def configure(conf):
	conf.check_tool ('gcc gnu_dirs')

def build(bld):
	if bld.is_install:
		bld (rule="""sed -e 's#@prefix@#${PREFIX}#' -e 's#@libdir@#${LIBDIR}#' -e 's#@bindir@#${BINDIR}#' -e 's#@version@#%s#' < ${SRC} > ${TGT}""" % VERSION,
		     source='msrpc-mingw-1.0.pc.in',
		     target='msrpc-mingw-1.0.pc',
		     install_path='${LIBDIR}/pkgconfig')

	bld.install_files('${BINDIR}', 'bin/midl-wrapper')
	bld.install_files('${DATADIR}/aclocal', 'm4macros/msrpc-mingw-1.0.m4')
