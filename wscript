import Logs

import os

APPNAME = 'msrpc-mingw'
VERSION = '0.1.0'
bld = 'build'
top = '.'

def options(opt):
	opt.tool_options('gcc gnu_dirs msrpc')

def configure(conf):
	# msrpc.py is normally used by users of the library; for our
	# purposes we want to force use of the in-tree version
	conf.env['MIDL_WRAPPER'] = os.path.abspath('bin/midl-wrapper')

	conf.check_tool ('gcc gnu_dirs msrpc')

	conf.check_cfg (package='glib-2.0', uselib_store='GLIB', args=['--cflags', '--libs'])

	conf.check_tool ('vala')

	# We do this manually; real users of the waf plugin will include the
	# pkg-config file which defines all this.
	conf.env['DEFINES_RPC'] = 'TARGET_IS_NT50_OR_LATER'
	conf.env['LIBPATH_RPC'] = 'd:/codethink/w32api-3.17-2-mingw32/lib'
	conf.env['INCLUDES'] = 'd:/codethink/w32api-3.17-2-mingw32/include'
	conf.env['LIB_RPC'] = ['rpcrt4']

	conf.env['CFLAGS'] = '-g'

	# Prevent gcc4 crashes
	conf.env['LDFLAGS'] = '-Wl,--enable-auto-import'

def build_libs (bld):
	msrpc_mingw_kws = {
		'source': 'src/msrpc-mingw.c',
		'target': 'msrpc-mingw',
		'uselib': 'RPC',
	}

	msrpc_glib2_kws = {
		'source': 'src/msrpc-glib2.c',
		'target': 'msrpc-glib2',
		'use': 'msrpc-mingw',
		'uselib': 'GLIB RPC',
	}

	bld (features = 'c cstlib', install_path = '${LIBDIR}', **msrpc_mingw_kws);
	bld (features = 'c cshlib', install_path = '${BINDIR}', **msrpc_mingw_kws);

	bld (features = 'c cstlib', install_path = '${LIBDIR}', **msrpc_glib2_kws);
	bld (features = 'c cshlib', install_path = '${BINDIR}', **msrpc_glib2_kws);


def build(bld):
	build_libs (bld)

	bld.install_files('${INCLUDEDIR}', 'src/msrpc-mingw.h')
	bld.install_files('${INCLUDEDIR}', 'src/msrpc-glib2.h')

	if bld.is_install:
		bld (rule="""sed -e 's#@prefix@#${PREFIX}#' -e 's#@libdir@#${LIBDIR}#' -e 's#@bindir@#${BINDIR}#' -e 's#@version@#%s#' < ${SRC} > ${TGT}""" % VERSION,
		     source='msrpc-mingw-1.0.pc.in',
		     target='msrpc-mingw-1.0.pc',
		     install_path='${LIBDIR}/pkgconfig')
		bld (rule="""sed -e 's#@prefix@#${PREFIX}#' -e 's#@libdir@#${LIBDIR}#' -e 's#@bindir@#${BINDIR}#' -e 's#@version@#%s#' < ${SRC} > ${TGT}""" % VERSION,
		     source='msrpc-glib2-1.0.pc.in',
		     target='msrpc-glib2-1.0.pc',
		     install_path='${LIBDIR}/pkgconfig')

	bld.install_files('${BINDIR}', 'bin/midl-wrapper')

	bld.install_files('${DATADIR}/vala-0.12/vapi', 'vapi/msrpc-1.0.vapi')

	bld.recurse ('tests')


def check (bld):
	# FIXME: it would be nice if check would automatically run build, but
	# it seems that 'recurse' can only happen once per execution. Maybe if
	# we could spoof the actual command invocation ...
	#bld.fun = 'build'
	#build (bld)

	# Which is why this is necessary ...
	build_libs (bld)

	tester = bld (features = 'c cprogram',
	              source   = 'tests/rpctester.c',
	              target   = 'rpctester',
	              use      = 'msrpc-mingw',
	              uselib   = 'RPC',
	              includes = 'src')

	bld.add_post_fun (check_action)

def check_action (bld):
	bld.recurse ('tests')

from waflib.Build import BuildContext
class check_context(BuildContext):
	cmd = 'check'
	fun = 'check'

	def run_tester(self, test_name, args):
		tester = self.get_tgen_by_name ('rpctester')
		tester_path = os.path.join (tester.path.get_bld().abspath(), tester.target)

		result = self.exec_command ("%s %s" % (tester_path, args))

		if result == 0:
			Logs.info ("%s: PASSED" % test_name)
		else:
			Logs.warn ("%s: FAILED" % test_name)
