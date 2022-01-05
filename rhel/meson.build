rhel_spec1 = vcs_tag(
    input : '../ndctl.spec.in',
    output : 'ndctl.spec.in',
    command: vcs_tagger,
    replace_string : 'VERSION',
)

rhel_spec2 = custom_target('ndctl.spec',
  command : [
    'sed', '-e', 's,DAX_DNAME,daxctl-devel,g',
	   '-e', 's,CXL_DNAME,cxl-devel,g',
	   '-e', 's,DNAME,ndctl-devel,g',
	   '-e', '/^%defattr.*/d',
	   '-e', 's,DAX_LNAME,daxctl-libs,g',
	   '-e', 's,CXL_LNAME,cxl-libs,g',
	   '-e', 's,LNAME,ndctl-libs,g',
	   '@INPUT@'
  ],
  input : rhel_spec1,
  output : 'ndctl.spec',
  capture : true,
)
