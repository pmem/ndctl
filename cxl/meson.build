cxl_src = [
  'cxl.c',
  'list.c',
  'port.c',
  'bus.c',
  'region.c',
  'memdev.c',
  'json.c',
  'filter.c',
  'event_trace.c',
  'monitor.c',
]

if get_option('systemd').enabled()
  install_data('cxl-monitor.service', install_dir : systemdunitdir)
endif

cxl_tool = executable('cxl',
  cxl_src,
  include_directories : root_inc,
  dependencies : [
    cxl_dep,
    util_dep,
    uuid,
    kmod,
    json,
    versiondep,
    traceevent,
    tracefs,
  ],
  install : true,
  install_dir : rootbindir,
)

install_headers('libcxl.h', subdir : 'cxl')
