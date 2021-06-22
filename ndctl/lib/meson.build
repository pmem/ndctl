libndctl_version = '@0@.@1@.@2@'.format(
  LIBNDCTL_CURRENT - LIBNDCTL_AGE,
  LIBNDCTL_REVISION,
  LIBNDCTL_AGE)

mapfile = files('libndctl.sym')
vflag = '-Wl,--version-script,@0@/@1@'.format(project_source_root, mapfile[0])

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
  link_args : vflag,
  link_depends : mapfile,
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
