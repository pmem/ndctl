if get_option('asciidoctor').enabled()
  asciidoc_conf = custom_target('asciidoctor-extensions.rb',
    command : [
      'sed', '-e', 's,@Utility@,Ndctl,g', '-e', 's,@utility@,ndctl,g', '@INPUT@'
    ],
    input : '../asciidoctor-extensions.rb.in',
    output : 'asciidoctor-extensions.rb',
    capture : true,
  )
else
  asciidoc_conf = custom_target('asciidoc.conf',
    command : [
      'sed', '-e', 's,UTILITY,ndctl,g',
    ],
    input : '../asciidoc.conf.in',
    output : 'asciidoc.conf',
    capture : true,
  )
endif

filedeps = [
        '../copyright.txt',
        'region-description.txt',
        'xable-region-options.txt',
        'dimm-description.txt',
        'xable-dimm-options.txt',
        'xable-namespace-options.txt',
        'ars-description.txt',
        'labels-description.txt',
        'labels-options.txt',
]

ndctl_manpages = [
  'ndctl.txt',
  'ndctl-wait-scrub.txt',
  'ndctl-start-scrub.txt',
  'ndctl-zero-labels.txt',
  'ndctl-read-labels.txt',
  'ndctl-write-labels.txt',
  'ndctl-init-labels.txt',
  'ndctl-check-labels.txt',
  'ndctl-enable-region.txt',
  'ndctl-disable-region.txt',
  'ndctl-enable-dimm.txt',
  'ndctl-disable-dimm.txt',
  'ndctl-enable-namespace.txt',
  'ndctl-disable-namespace.txt',
  'ndctl-create-namespace.txt',
  'ndctl-destroy-namespace.txt',
  'ndctl-check-namespace.txt',
  'ndctl-clear-errors.txt',
  'ndctl-inject-error.txt',
  'ndctl-inject-smart.txt',
  'ndctl-update-firmware.txt',
  'ndctl-list.txt',
  'ndctl-monitor.txt',
  'ndctl-setup-passphrase.txt',
  'ndctl-update-passphrase.txt',
  'ndctl-remove-passphrase.txt',
  'ndctl-freeze-security.txt',
  'ndctl-sanitize-dimm.txt',
  'ndctl-load-keys.txt',
  'ndctl-wait-overwrite.txt',
  'ndctl-read-infoblock.txt',
  'ndctl-write-infoblock.txt',
  'ndctl-activate-firmware.txt',
]

foreach man : ndctl_manpages
  name = man.split('.')[0]
  output = name + '.1'
  output_xml = name + '.xml'
  if get_option('asciidoctor').enabled()
    custom_target(name,
      command : [
        asciidoc,
        '-b', 'manpage', '-d', 'manpage', '-acompat-mode', '-I', '@OUTDIR@',
        '-rasciidoctor-extensions', '-amansource=ndctl',
        '-amanmanual=ndctl Manual',
	'-andctl_confdir=@0@'.format(ndctlconf_dir),
	'-andctl_monitorconf=@0@'.format(ndctlconf),
	'-andctl_keysdir=@0@'.format(ndctlkeys_dir),
        '-andctl_version=@0@'.format(meson.project_version()),
        '-o', '@OUTPUT@', '@INPUT@'
      ],
      input : man,
      output : output,
      depend_files : filedeps,
      depends : asciidoc_conf,
      install : get_option('docs').enabled(),
      install_dir : join_paths(get_option('mandir'), 'man1'),
    )
  else
    xml = custom_target(output_xml,
      command : [
        asciidoc,
	'-b', 'docbook', '-d', 'manpage', '-f', asciidoc_conf, '--unsafe',
	'-andctl_version=@0@'.format(meson.project_version()),
	'-andctl_confdir=@0@'.format(ndctlconf_dir),
	'-andctl_monitorconf=@0@'.format(ndctlconf),
	'-andctl_keysdir=@0@'.format(ndctlkeys_dir),
	'-o', '@OUTPUT@', '@INPUT@',
      ],
      input : man,
      output : output_xml,
      depend_files : filedeps,
      depends : asciidoc_conf,
    )

    xsl = files('../manpage-normal.xsl')

    custom_target(name,
      command : [
        xmlto, '-o', '@OUTDIR@', '-m', xsl, 'man', '@INPUT@'
      ],
      depends : xml,
      depend_files : xsl,
      input : xml,
      output : output,
      install : get_option('docs').enabled(),
      install_dir : join_paths(get_option('mandir'), 'man1'),
    )
  endif
endforeach
