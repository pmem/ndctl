daxctl_src = [
  'daxctl.c',
  'acpi.c',
  'list.c',
  'migrate.c',
  'device.c',
  '../util/json.c',
  'json.c',
  'filter.c',
]

daxctl_tool = executable('daxctl',
  daxctl_src,
  include_directories : root_inc,
  dependencies : [
    daxctl_dep,
    ndctl_dep,
    util_dep,
    uuid,
    kmod,
    json,
    versiondep,
  ],
  install : true,
  install_dir : rootbindir,
)

install_headers('libdaxctl.h', subdir : 'daxctl')

install_data('daxctl.example.conf', install_dir : daxctlconf_dir )
if get_option('systemd').enabled()
  install_data('90-daxctl-device.rules', install_dir : udevrulesdir)
  install_data('daxdev-reconfigure@.service', install_dir : systemdunitdir)
endif
