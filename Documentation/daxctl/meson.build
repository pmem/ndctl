if get_option('asciidoctor').enabled()
  asciidoc_conf = custom_target('asciidoctor-extensions.rb',
    command : [
      'sed', '-e', 's,@Utility@,Daxctl,g', '-e', 's,@utility@,daxctl,g', '@INPUT@'
    ],
    input : '../asciidoctor-extensions.rb.in',
    output : 'asciidoctor-extensions.rb',
    capture : true,
  )
else
  asciidoc_conf = custom_target('asciidoc.conf',
    command : [
      'sed', '-e', 's,UTILITY,daxctl,g',
    ],
    input : '../asciidoc.conf.in',
    output : 'asciidoc.conf',
    capture : true,
  )
endif

filedeps = [
	'human-option.txt',
        '../copyright.txt',
]

daxctl_manpages = [
  'daxctl.txt',
  'daxctl-list.txt',
  'daxctl-migrate-device-model.txt',
  'daxctl-reconfigure-device.txt',
  'daxctl-online-memory.txt',
  'daxctl-offline-memory.txt',
  'daxctl-disable-device.txt',
  'daxctl-enable-device.txt',
  'daxctl-create-device.txt',
  'daxctl-destroy-device.txt',
]

foreach man : daxctl_manpages
  name = man.split('.')[0]
  output = name + '.1'
  output_xml = name + '.xml'
  if get_option('asciidoctor').enabled()
    custom_target(name,
      command : [
        asciidoc,
        '-b', 'manpage', '-d', 'manpage', '-acompat-mode', '-I', '@OUTDIR@',
        '-rasciidoctor-extensions', '-amansource=daxctl',
        '-amanmanual=daxctl Manual',
	'-adaxctl_confdir=@0@'.format(daxctlconf_dir),
	'-adaxctl_conf=@0@'.format(daxctlconf),
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
	'-adaxctl_confdir=@0@'.format(daxctlconf_dir),
	'-adaxctl_conf=@0@'.format(daxctlconf),
	'-andctl_keysdir=@0@'.format(ndctlkeys_dir),
	'-andctl_version=@0@'.format(meson.project_version()),
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
