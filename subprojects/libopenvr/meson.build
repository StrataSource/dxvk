project('libopenvr', 'cpp', version: '0.0.0', license: 'MIT', meson_version: '>= 0.55.0', default_options: ['cpp_std=c++17', 'b_vscrt=static_from_buildtype'])

add_project_arguments(['-DVRCORE_NO_PLATFORM', '-DVR_API_PUBLIC', '-DOPENVR_BUILD_STATIC', '-DPOSIX', '-DLINUX'], language: 'cpp')

ovr_lib = static_library(
	'openvr',
	[
		'openvr_api_public.cpp',
		'jsoncpp.cpp',
		'vrcore/dirtools_public.cpp',
		'vrcore/envvartools_public.cpp',
		'vrcore/pathtools_public.cpp',
		'vrcore/sharedlibtools_public.cpp',
		'vrcore/hmderrors_public.cpp',
		'vrcore/vrpathregistry_public.cpp',
		'vrcore/strtools_public.cpp'
	],
	include_directories: include_directories('include', 'vrcore'),
	install: false,
)

ovr_dep = declare_dependency(
    compile_args: ['-DOPENVR_BUILD_STATIC'],
	link_with: ovr_lib,
	include_directories: include_directories('include'),
)
