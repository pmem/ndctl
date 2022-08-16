if get_option('asciidoctor').enabled()
  asciidoc_conf = custom_target('asciidoctor-extensions.rb',
    command : [
      'sed', '-e', 's,@Utility@,Cxl,g', '-e', 's,@utility@,cxl,g', '@INPUT@'
    ],
    input : '../asciidoctor-extensions.rb.in',
    output : 'asciidoctor-extensions.rb',
    capture : true,
  )
else
  asciidoc_conf = custom_target('asciidoc.conf',
    command : [
      'sed', '-e', 's,UTILITY,cxl,g',
    ],
    input : '../asciidoc.conf.in',
    output : 'asciidoc.conf',
    capture : true,
  )
endif

filedeps = [
  '../copyright.txt',
  'memdev-option.txt',
  'labels-options.txt',
]

cxl_manpages = [
  'cxl.txt',
  'cxl-list.txt',
  'cxl-read-labels.txt',
  'cxl-write-labels.txt',
  'cxl-zero-labels.txt',
  'cxl-enable-memdev.txt',
  'cxl-disable-memdev.txt',
  'cxl-enable-port.txt',
  'cxl-disable-port.txt',
  'cxl-disable-bus.txt',
  'cxl-set-partition.txt',
  'cxl-reserve-dpa.txt',
  'cxl-free-dpa.txt',
]

foreach man : cxl_manpages
  name = man.split('.')[0]
  output = name + '.1'
  output_xml = name + '.xml'
  if get_option('asciidoctor').enabled()
    custom_target(name,
      command : [
        asciidoc,
        '-b', 'manpage', '-d', 'manpage', '-acompat-mode', '-I', '@OUTDIR@',
        '-rasciidoctor-extensions', '-amansource=cxl',
        '-amanmanual=cxl Manual',
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

subdir('lib')
