project('libmobile', 'c',
  version : '0.2.0',
  default_options : [
    'warning_level=2',
    'c_std=c11',
    'default_library=both'])

conf = configuration_data({
  'prefix': get_option('prefix'),
  'exec_prefix': '${prefix}',
  'libdir': '${exec_prefix}' / get_option('libdir'),
  'includedir': '${prefix}' / get_option('includedir'),
  'PACKAGE': meson.project_name(),
  'VERSION': meson.project_version(),

  'MOBILE_ENABLE_IMPL_WEAK': get_option('enable_impl_weak'),
  'MOBILE_ENABLE_NOALLOC': get_option('enable_noalloc')
})

c_args = [
  '-DMOBILE_LIBCONF_USE'
]

if not get_option('debug')
  c_args += '-DNDEBUG'
endif

configure_file(
  input : 'mobile_config.meson.h.in',
  output : 'mobile_config.h',
  configuration : conf)

configure_file(
  input : 'libmobile.pc.in',
  output : 'libmobile.pc',
  configuration : conf,
  install : true,
  install_dir : get_option('libdir') / 'pkgconfig')

libmobile = library('mobile',
  'alloc.c',
  'callback.c',
  'commands.c',
  'config.c',
  'debug.c',
  'dns.c',
  'inet_pton.c',
  'mobile.c',
  'relay.c',
  'serial.c',
  'util.c',
  c_args : c_args,
  pic : false,
  install : true,
  version : '0.0.0')

install_headers(
    'mobile.h',
    'mobile_inet.h')

libmobile_dep = declare_dependency(
  link_with : libmobile,
  include_directories: '.')
meson.override_dependency('libmobile', libmobile_dep)
