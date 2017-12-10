project('hex', 'cpp', 
	license: 'BSL',
	version: '0.1.0',
	meson_version: '>=0.43',
	default_options: [
		'cpp_std=c++1z', 
  		'warning_level=3', 
  		'werror=true'])

warnings = [
	'-Wall',
	'-Wextra',
	'-Wpedantic',

	# extra
	'-Wduplicated-cond',
	'-Wrestrict',
	'-Wnull-dereference',
	'-Wundef',
	'-Wcast-align=strict',
	'-Wlogical-op',

	# needed on clang
	'-Wno-missing-braces',

	# not a good default usually, but might spot errors,
	# so comment this out at times
	'-Wno-non-virtual-dtor',

	# potentially useful (but usually problematic with c (headers))
	# '-Wzero-as-null-pointer-constant',
	# '-Wold-style-cast',
]

# default arrguments
# warnings and stuff
cc = meson.get_compiler('cpp')
add_project_arguments(
	cc.get_supported_arguments(warnings),
	language: 'cpp')


# project-specific stuff
source_root = meson.source_root().split('\\')
add_project_arguments('-DDLG_BASE_PATH="' + '/'.join(source_root) + '/"', language: 'cpp')

# workaround until dlg 0.3
add_project_arguments('-DDLG_DISABLE=0', language: 'cpp')

dep_vpp = dependency('vpp', fallback: ['vpp', 'vpp_dep'])
dep_ny = dependency('ny', fallback: ['ny', 'ny_dep'])
dep_vulkan = dependency('vulkan')

subdir('assets/shaders')
shader_inc = include_directories('assets') # for headers in build folder

src = [
	shaders,
	'main.cpp',
	'render.cpp',
	'window.cpp']

executable('hex', src,
	dependencies: [dep_vpp, dep_vulkan, dep_ny],
	include_directories: shader_inc)
