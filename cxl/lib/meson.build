libcxl_version = '@0@.@1@.@2@'.format(
  LIBCXL_CURRENT - LIBCXL_AGE,
  LIBCXL_REVISION,
  LIBCXL_AGE)

libcxl_dir_path = meson.current_source_dir()
libcxl_sym = files('libcxl.sym')
libcxl_sym_path = libcxl_dir_path / 'libcxl.sym'

cxl = library('cxl',
  '../../util/sysfs.c',
  '../../util/log.c',
  '../../util/log.h',
  'libcxl.c',
  include_directories : root_inc,
  dependencies : [
    uuid,
    kmod,
    libudev,
    daxctl_dep,
  ],
  version : libcxl_version,
  install : true,
  install_dir : rootlibdir,
  link_args : '-Wl,--version-script=' + libcxl_sym_path,
  link_depends : libcxl_sym,
)
cxl_dep = declare_dependency(link_with : cxl)

custom_target(
  'libcxl.pc',
  command : pkgconfig_script + [ '@INPUT@' ],
  input : 'libcxl.pc.in',
  output : 'libcxl.pc',
  capture : true,
  install : true,
  install_dir : pkgconfiglibdir,
)
