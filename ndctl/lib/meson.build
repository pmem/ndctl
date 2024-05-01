libndctl_version = '@0@.@1@.@2@'.format(
  LIBNDCTL_CURRENT - LIBNDCTL_AGE,
  LIBNDCTL_REVISION,
  LIBNDCTL_AGE)

libndctl_dir_path = meson.current_source_dir()
libndctl_sym = files('libndctl.sym')
libndctl_sym_path = libndctl_dir_path / 'libndctl.sym'

ndctl = library(
 'ndctl',
  '../../util/log.c',
  '../../util/sysfs.c',
  'dimm.c',
  'inject.c',
  'nfit.c',
  'smart.c',
  'intel.c',
  'hpe1.c',
  'msft.c',
  'hyperv.c',
  'papr.c',
  'ars.c',
  'firmware.c',
  'libndctl.c',
  dependencies : [
    daxctl_dep,
    libudev,
    uuid,
    kmod,
  ],
  include_directories : root_inc,
  version : libndctl_version,
  install : true,
  install_dir : rootlibdir,
  link_args : '-Wl,--version-script=' + libndctl_sym_path,
  link_depends : libndctl_sym,
)
ndctl_dep = declare_dependency(link_with : ndctl)

custom_target(
  'libndctl.pc',
  command : pkgconfig_script + [ '@INPUT@' ],
  input : 'libndctl.pc.in',
  output : 'libndctl.pc',
  capture : true,
  install : true,
  install_dir : pkgconfiglibdir,
)
