ndctl_src = [
  'ndctl.c',
  'bus.c',
  'create-nfit.c',
  'namespace.c',
  'check.c',
  'region.c',
  'dimm.c',
  '../daxctl/filter.c',
  'filter.c',
  'list.c',
  '../daxctl/json.c',
  'json.c',
  'json-smart.c',
  'inject-error.c',
  'inject-smart.c',
  'monitor.c',
]

deps = [
  util_dep,
  ndctl_dep,
  daxctl_dep,
  cxl_dep,
  uuid,
  kmod,
  json,
  versiondep,
]

if get_option('keyutils').enabled()
  ndctl_src += [
    'keys.c',
    'load-keys.c',
  ]
  deps += keyutils
endif

if get_option('test').enabled()
  ndctl_src += [
  '../test/libndctl.c',
  '../test/dsm-fail.c',
  '../util/sysfs.c',
  '../test/core.c',
  'test.c',
]
endif

if get_option('destructive').enabled()
  if get_option('test').disabled()
    error('\'-D=destructive=enabled\' requires \'-Dtest=enabled\'\n')
  endif
  ndctl_src += [
    '../test/pmem_namespaces.c',
    'bat.c',
  ]
endif

if get_option('systemd').enabled()
  install_data('ndctl-monitor.service', install_dir : systemdunitdir)
endif
install_data('monitor.conf', install_dir : ndctlconf_dir)
install_data('ndctl.conf', install_dir : ndctlconf_dir)
install_data('keys.readme', install_dir : ndctlkeys_dir)

ndctl_tool = executable('ndctl', ndctl_src,
  dependencies : deps,
  install : true,
  install_dir : rootbindir,
  include_directories : root_inc,
)

install_headers(
  [
    'libndctl.h',
    'ndctl.h'
  ],
  subdir : 'ndctl'
)
