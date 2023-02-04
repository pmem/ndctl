cxl_src = [
  'cxl.c',
  'list.c',
  'port.c',
  'bus.c',
  'region.c',
  'memdev.c',
  'json.c',
  'filter.c',
  '../daxctl/json.c',
  '../daxctl/filter.c',
]

cxl_tool = executable('cxl',
  cxl_src,
  include_directories : root_inc,
  dependencies : [
    cxl_dep,
    daxctl_dep,
    util_dep,
    uuid,
    kmod,
    json,
    versiondep,
  ],
  install : true,
  install_dir : rootbindir,
)

install_headers('libcxl.h', subdir : 'cxl')
