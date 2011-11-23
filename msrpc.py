##
## Waf support for midl
##
## To use:
##   def configure(conf):
##     conf.check_tool ('gcc msrpc')
##
##   def build(bld):
##     client = bld (features = 'c cprogram msrpc_client',
##                   msrpc_interface = ['src/interface.idl', 'src/interface.acl'],
##                   ...)
##
## FIXME: could submit this upstream?
##

import os
from waflib import Task
from waflib.TaskGen import before_method, feature, taskgen_method

def task_output_already_exists(self, output):
	for group in self.bld.groups:
		for task_gen in group:
			for task in getattr(task_gen, 'tasks', []):
				if output in task.outputs:
					return True
	return False

def process_msrpc_interface(self, type):
	"""
	Process the IDL files in *msrpc_interface_for_client* or
	*msrpc_interface_for_server* and generate C stubs that will
	be linked to the target.
	"""
	if not hasattr(self, 'msrpc_interface'):
		return

	source_list = self.to_nodes(self.msrpc_interface)

	idl_file_node = source_list[0]
	if type=='server':
		c_stub_node = idl_file_node.change_ext('_s.c').get_bld()
	elif type=='client':
		c_stub_node = idl_file_node.change_ext('_c.c').get_bld()

	c_header = idl_file_node.change_ext('.h').get_bld()

	# Avoid any duplicate task errors - it's not invalid to have two
	# client programs generated from the same .IDL file, for example,
	# but we only need to generate the stub C code once.

	output_list = []
	if not task_output_already_exists(self, c_stub_node):
		output_list.append (c_stub_node)
	if not task_output_already_exists(self, c_header):
		output_list.append (c_header) 

	midl_task = self.create_task('midl', source_list, output_list)
	midl_task.env['MIDL_WRAPPER_OPTIONS'] = ["-o", "%s" % c_stub_node.bld_dir()]
	midl_task.env['MIDL_INPUTS'] = [k.abspath() for k in source_list]

	self.source = self.to_nodes(getattr(self, 'source', []))
	self.source.append(c_stub_node)


@feature('msrpc_server')
@before_method('process_source')
def process_msrpc_interface_server(self):
    process_msrpc_interface(self, 'server')

@feature('msrpc_client')
@before_method('process_source')
def process_msrpc_interface_client(self):
    process_msrpc_interface(self, 'client')

class midl(Task.Task):
	"""
	Process .IDL files with MIDL.EXE
	"""
	run_str = 'sh ${MIDL_WRAPPER} -m ${MIDL} ${MIDL_WRAPPER_OPTIONS} ${MIDL_INPUTS}'
	vars = ['MIDL']
	color = 'PINK'

def configure(conf):
	conf.find_program('midl-wrapper', var='MIDL_WRAPPER')
	conf.find_program('midl', var='MIDL', path_list=['c:/Program Files/Microsoft SDKs/Windows/v7.0/Bin'])


# Auxiliary function
@taskgen_method
def add_msrpc_interface(self, filename_list):
	"""
	Add MSRPC interface definitions, as *msrpc_interface*

	:param filename_list: files
	:type filename_list: list of string
	"""
	if hasattr(self, 'msrpc_interface'):
		raise Errors.WafError("Only one MSRPC interface permitted per target, on '%s'." % self.name)

	self.msrpc_interface = filename_list
