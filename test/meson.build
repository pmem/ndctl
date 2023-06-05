testcore = [
  'core.c',
  '../util/log.c',
  '../util/sysfs.c',
]

libndctl_deps = [
  ndctl_dep,
  daxctl_dep,
  uuid,
  kmod,
]

ndctl_deps = libndctl_deps + [
  json,
  util_dep,
  versiondep,
]

libndctl = executable('libndctl', testcore + [ 'libndctl.c'],
  dependencies : libndctl_deps,
  include_directories : root_inc,
)

namespace_core = [
  '../ndctl/namespace.c',
  '../ndctl/filter.c',
  '../ndctl/check.c',
  '../util/json.c',
  '../ndctl/json.c',
  '../daxctl/filter.c',
  '../daxctl/json.c',
]

dsm_fail = executable('dsm-fail', testcore + namespace_core + [ 'dsm-fail.c' ],
  dependencies : ndctl_deps,
  include_directories : root_inc,
)

hugetlb_src = testcore + [ 'hugetlb.c', 'dax-pmd.c' ]
if poison_enabled
  hugetlb_src += [ 'dax-poison.c' ]
endif
hugetlb = executable('hugetlb', hugetlb_src,
  dependencies : libndctl_deps,
  include_directories : root_inc,
)

ack_shutdown_count = executable('ack-shutdown-count-set',
  testcore + [ 'ack-shutdown-count-set.c' ],
  dependencies : libndctl_deps,
  include_directories : root_inc,
)

dax_errors = executable('dax-errors',
  'dax-errors.c',
)

smart_notify = executable('smart-notify', 'smart-notify.c',
  dependencies : libndctl_deps,
  include_directories : root_inc,
)

smart_listen = executable('smart-listen', 'smart-listen.c',
  dependencies : libndctl_deps,
  include_directories : root_inc,
)

daxdev_errors = executable('daxdev-errors', [
    'daxdev-errors.c',
    '../util/log.c',
    '../util/sysfs.c',
  ],
  dependencies : libndctl_deps,
  include_directories : root_inc,
)

list_smart_dimm = executable('list-smart-dimm', [
    'list-smart-dimm.c',
    '../ndctl/filter.c',
    '../util/json.c',
    '../ndctl/json.c',
    '../daxctl/json.c',
    '../daxctl/filter.c',
  ],
  dependencies : ndctl_deps,
  include_directories : root_inc,
)

pmem_ns = executable('pmem-ns', testcore + [ 'pmem_namespaces.c' ],
  dependencies : libndctl_deps,
  include_directories : root_inc,
)

dax_dev = executable('dax-dev', testcore + [ 'dax-dev.c' ],
  dependencies : libndctl_deps,
  include_directories : root_inc,
)

dax_pmd_src = testcore + [ 'dax-pmd.c' ]
if poison_enabled
  dax_pmd_src += [ 'dax-poison.c' ]
endif

dax_pmd = executable('dax-pmd', dax_pmd_src,
  dependencies : libndctl_deps,
  include_directories : root_inc,
)

device_dax_src = testcore + namespace_core + [
  'device-dax.c',
  'dax-dev.c',
  'dax-pmd.c',
]

if poison_enabled
  device_dax_src += 'dax-poison.c'
endif

device_dax = executable('device-dax', device_dax_src,
  dependencies : ndctl_deps,
  include_directories : root_inc,
)

revoke_devmem = executable('revoke_devmem', testcore + [
    'revoke-devmem.c',
    'dax-dev.c',
  ],
  dependencies : libndctl_deps,
  include_directories : root_inc,
)

mmap = executable('mmap', 'mmap.c',)

create = find_program('create.sh')
clear = find_program('clear.sh')
pmem_errors = find_program('pmem-errors.sh')
daxdev_errors_sh = find_program('daxdev-errors.sh')
multi_dax = find_program('multi-dax.sh')
btt_check = find_program('btt-check.sh')
label_compat = find_program('label-compat.sh')
sector_mode = find_program('sector-mode.sh')
inject_error = find_program('inject-error.sh')
btt_errors = find_program('btt-errors.sh')
btt_pad_compat = find_program('btt-pad-compat.sh')
firmware_update = find_program('firmware-update.sh')
rescan_partitions = find_program('rescan-partitions.sh')
inject_smart = find_program('inject-smart.sh')
monitor = find_program('monitor.sh')
max_extent = find_program('max_available_extent_ns.sh')
pfn_meta_errors = find_program('pfn-meta-errors.sh')
track_uuid = find_program('track-uuid.sh')
cxl_topo = find_program('cxl-topology.sh')
cxl_sysfs = find_program('cxl-region-sysfs.sh')
cxl_labels = find_program('cxl-labels.sh')
cxl_create_region = find_program('cxl-create-region.sh')
cxl_xor_region = find_program('cxl-xor-region.sh')
cxl_update_firmware = find_program('cxl-update-firmware.sh')

tests = [
  [ 'libndctl',               libndctl,		  'ndctl' ],
  [ 'dsm-fail',               dsm_fail,	      	  'ndctl' ],
  [ 'create.sh',              create,	      	  'ndctl' ],
  [ 'clear.sh',               clear,	      	  'ndctl' ],
  [ 'pmem-errors.sh',         pmem_errors,    	  'ndctl' ],
  [ 'daxdev-errors.sh',       daxdev_errors_sh,	  'dax'	  ],
  [ 'multi-dax.sh',           multi_dax,	  'dax'   ],
  [ 'btt-check.sh',           btt_check,	  'ndctl' ],
  [ 'label-compat.sh',        label_compat,       'ndctl' ],
  [ 'sector-mode.sh',         sector_mode,        'ndctl' ],
  [ 'inject-error.sh',        inject_error,	  'ndctl' ],
  [ 'btt-errors.sh',          btt_errors,	  'ndctl' ],
  [ 'hugetlb',                hugetlb,		  'ndctl' ],
  [ 'btt-pad-compat.sh',      btt_pad_compat,	  'ndctl' ],
  [ 'ack-shutdown-count-set', ack_shutdown_count, 'ndctl' ],
  [ 'rescan-partitions.sh',   rescan_partitions,  'ndctl' ],
  [ 'inject-smart.sh',        inject_smart,	  'ndctl' ],
  [ 'monitor.sh',             monitor,		  'ndctl' ],
  [ 'max_extent_ns',          max_extent,	  'ndctl' ],
  [ 'pfn-meta-errors.sh',     pfn_meta_errors,	  'ndctl' ],
  [ 'track-uuid.sh',          track_uuid,	  'ndctl' ],
  [ 'cxl-topology.sh',	      cxl_topo,		  'cxl'   ],
  [ 'cxl-region-sysfs.sh',    cxl_sysfs,	  'cxl'   ],
  [ 'cxl-labels.sh',          cxl_labels,	  'cxl'   ],
  [ 'cxl-create-region.sh',   cxl_create_region,  'cxl'   ],
  [ 'cxl-xor-region.sh',      cxl_xor_region,     'cxl'   ],
]

if get_option('destructive').enabled()
  sub_section = find_program('sub-section.sh')
  dax_ext4 = find_program('dax-ext4.sh')
  dax_xfs = find_program('dax-xfs.sh')
  align = find_program('align.sh')
  device_dax_fio = find_program('device-dax-fio.sh')
  daxctl_devices = find_program('daxctl-devices.sh')
  daxctl_create = find_program('daxctl-create.sh')
  dm = find_program('dm.sh')
  mmap_test = find_program('mmap.sh')

  tests += [
    [ 'firmware-update.sh',     firmware_update,	  'ndctl' ],
    [ 'cxl-update-firmware.sh', cxl_update_firmware,      'cxl'   ],
    [ 'pmem-ns',           pmem_ns,	   'ndctl' ],
    [ 'sub-section.sh',    sub_section,	   'dax'   ],
    [ 'dax-dev',           dax_dev,	   'dax'   ],
    [ 'dax-ext4.sh',       dax_ext4,	   'dax'   ],
    [ 'dax-xfs.sh',        dax_xfs,	   'dax'   ],
    [ 'align.sh',          align,	   'ndctl' ],
    [ 'device-dax',        device_dax,	   'dax'   ],
    [ 'revoke-devmem',     revoke_devmem,  'dax'   ],
    [ 'device-dax-fio.sh', device_dax_fio, 'dax'   ],
    [ 'daxctl-devices.sh', daxctl_devices, 'dax'   ],
    [ 'daxctl-create.sh',  daxctl_create,  'dax'   ],
    [ 'dm.sh',             dm,		   'dax'   ],
    [ 'mmap.sh',           mmap_test,	   'dax'   ],
  ]
endif

if get_option('keyutils').enabled()
  nfit_security = find_program('nfit-security.sh')
  cxl_security = find_program('cxl-security.sh')
  tests += [
    [ 'nfit-security.sh', nfit_security, 'ndctl' ],
    [ 'cxl-security.sh', cxl_security, 'cxl' ],
  ]
endif

foreach t : tests
  test(t[0], t[1],
    is_parallel : false,
    depends : [
      ndctl_tool,
      daxctl_tool,
      cxl_tool,
      smart_notify,
      list_smart_dimm,
      dax_pmd,
      dax_errors,
      daxdev_errors,
      dax_dev,
      mmap,
    ],
    suite: t[2],
    timeout : 600,
    env : [
      'NDCTL=@0@'.format(ndctl_tool.full_path()),
      'DAXCTL=@0@'.format(daxctl_tool.full_path()),
      'TEST_PATH=@0@'.format(meson.current_build_dir()),
      'DATA_PATH=@0@'.format(meson.current_source_dir()),
    ],
  )
endforeach
