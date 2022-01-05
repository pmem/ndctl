libdaxctl_version = '@0@.@1@.@2@'.format(
  LIBDAXCTL_CURRENT - LIBDAXCTL_AGE,
  LIBDAXCTL_REVISION,
  LIBDAXCTL_AGE,
)

mapfile = files('libdaxctl.sym')
vflag = '-Wl,--version-script,@0@/@1@'.format(project_source_root, mapfile[0])

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
  link_args : vflag,
  link_depends : mapfile,
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
