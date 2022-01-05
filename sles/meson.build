sles_spec1 = vcs_tag(
    input : '../ndctl.spec.in',
    output : 'ndctl.spec.sles.in',
    command: vcs_tagger,
    replace_string : 'VERSION',
)

header = files('header')

sles_spec2 = custom_target('ndctl.spec.in',
  command : [
    'cat', header, '@INPUT@',
  ],
  input : sles_spec1,
  output : 'ndctl.spec.in',
  capture : true,
)

sles_spec3 = custom_target('ndctl.spec',
  command : [
    'sed', '-e', 's,MESON,1,g',
           '-e', 's,DAX_DNAME,libdaxctl-devel,g',
           '-e', 's,CXL_DNAME,libcxl-devel,g',
           '-e', 's,DNAME,libndctl-devel,g',
           '-e', 's,%license,%doc,g',
           '-e', 's,\(^License:.*GPL\)v2,\1-2.0,g',
           '-e', 's,DAX_LNAME,libdaxctl@0@,g'.format(LIBDAXCTL_CURRENT - LIBDAXCTL_AGE),
           '-e', 's,CXL_LNAME,libcxl@0@,g'.format(LIBCXL_CURRENT - LIBCXL_AGE),
           '-e', 's,LNAME,libndctl@0@,g'.format(LIBNDCTL_CURRENT - LIBNDCTL_AGE),
	   '@INPUT@'
  ],

  input : sles_spec2,
  output : 'ndctl.spec',
  capture : true,
)
