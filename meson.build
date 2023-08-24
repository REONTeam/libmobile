project('libmobile', 'c',
  version : '0.2.0',
  default_options : [
    'warning_level=2',
    'c_std=c11',
    'default_library=both'])

# https://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html
lt_current  = 0
lt_revision = 0
lt_age      = 0
lt_soversion = lt_current - lt_age
lt_version = '@0@.@1@.@2@'.format(lt_soversion, lt_current, lt_age)

c_args = [
  '-DMOBILE_LIBCONF_USE'
]

if not get_option('debug')
  c_args += '-DNDEBUG'
endif

conf = configuration_data({
  'prefix': get_option('prefix'),
  'exec_prefix': '${prefix}',
  'libdir': '${exec_prefix}' / get_option('libdir'),
  'includedir': '${prefix}' / get_option('includedir'),
  'PACKAGE': meson.project_name(),
  'VERSION': meson.project_version(),

  'MOBILE_ENABLE_IMPL_WEAK': get_option('enable_impl_weak'),
  'MOBILE_ENABLE_NOALLOC': get_option('enable_noalloc'),
  'MOBILE_ENABLE_NO32BIT': get_option('enable_no32bit'),
  'MOBILE_TIME_SENSITIVE': get_option('mobile_time_sensitive')
})

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

sources = [
  'atomic.h',
  'callback.c',
  'callback.h',
  'commands.c',
  'commands.h',
  'compat.h',
  'config.c',
  'config.h',
  'debug.c',
  'debug.h',
  'dns.c',
  'dns.h',
  'global.h',
  'inet_pton.c',
  'mobile.c',
  'mobile_data.h',
  'relay.c',
  'relay.h',
  'serial.c',
  'serial.h',
  'util.c',
  'util.h'
]

headers = [
  'mobile.h',
  'mobile_inet.h'
]

libmobile = library('mobile',
  sources,
  headers,
  c_args : c_args,
  pic : false,
  install : true,
  version : lt_version)
install_headers(headers)

libmobile_dep = declare_dependency(
  link_with : libmobile,
  compile_args : ['-DMOBILE_LIBCONF_USE'],
  include_directories: '.')
meson.override_dependency('libmobile', libmobile_dep)
