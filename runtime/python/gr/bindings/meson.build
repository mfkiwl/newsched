runtime_pybind_sources = files([
    'block_work_io_pybind.cc',
    'port_pybind.cc',
    'tag_pybind.cc',
    'block_pybind.cc',
    'node_pybind.cc',
    'sync_block_pybind.cc',
    'edge_pybind.cc',
    'graph_pybind.cc',
    'flowgraph_pybind.cc',
    'runtime_pybind.cc',
    'scheduler_pybind.cc',
    'buffer_pybind.cc',
    'domain_pybind.cc',
    'buffer_cpu_vmcirc_pybind.cc',
    'constants_pybind.cc',
    'python_block_pybind.cc',
    'pyblock_detail_pybind.cc'
 ] )

cpp_args = []
runtime_pybind_deps = [newsched_runtime_dep, python3_dep, pybind11_dep, pmtf_dep]
if USE_CUDA
    runtime_pybind_sources += [
        'buffer_cuda_pybind.cc', 
        'buffer_cuda_pinned.cc',
        'buffer_cuda_sm_pybind.cc']
    runtime_pybind_deps += cuda_dep
    cpp_args = '-DHAVE_CUDA'
endif
newsched_runtime_pybind = py3_inst.extension_module('runtime_python',
    runtime_pybind_sources, 
    dependencies : runtime_pybind_deps,
    link_language : 'cpp',
    install : true,
    cpp_args : cpp_args,
    install_dir : join_paths(py3_inst.get_install_dir(),'newsched','gr')
)

newsched_runtime_pybind_dep = declare_dependency(
					   link_with : newsched_runtime_pybind)
