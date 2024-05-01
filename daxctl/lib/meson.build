libdaxctl_version = '@0@.@1@.@2@'.format(
  LIBDAXCTL_CURRENT - LIBDAXCTL_AGE,
  LIBDAXCTL_REVISION,
  LIBDAXCTL_AGE,
)

libdaxctl_dir_path = meson.current_source_dir()
libdaxctl_sym = files('libdaxctl.sym')
libdaxctl_sym_path = libdaxctl_dir_path / 'libdaxctl.sym'


libdaxctl_src = [
  '../../util/iomem.c',
  '../../util/sysfs.c',
  '../../util/log.c',
  'libdaxctl.c',
]

daxctl = library(
 'daxctl',
  libdaxctl_src,
  version : libdaxctl_version,
  include_directories : root_inc,
  dependencies : [
    uuid,
    kmod,
  ],
  install : true,
  install_dir : rootlibdir,
  link_args : '-Wl,--version-script=' + libdaxctl_sym_path,
  link_depends : libdaxctl_sym,
)

daxctl_dep = declare_dependency(link_with : daxctl)

custom_target(
  'libdaxctl.pc',
  command : pkgconfig_script + [ '@INPUT@' ],
  input : 'libdaxctl.pc.in',
  output : 'libdaxctl.pc',
  capture : true,
  install : true,
  install_dir : pkgconfiglibdir,
)

install_data('daxctl.conf', install_dir : datadir / 'daxctl')
