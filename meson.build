project('ndctl', 'c',
  version : '73',
  license : [
    'GPL-2.0',
    'LGPL-2.1',
    'CC0-1.0',
    'MIT',
  ],
  default_options : [
    'c_std=gnu99',
    'prefix=/usr',
    'sysconfdir=/etc',
    'localstatedir=/var',
  ],
)

# rootprefixdir and rootlibdir setup copied from systemd:
rootprefixdir = get_option('rootprefix')
rootprefix_default = '/usr'
if rootprefixdir == ''
        rootprefixdir = rootprefix_default
endif
rootbindir = join_paths(rootprefixdir, 'bin')

# join_paths ignores the preceding arguments if an absolute component is
# encountered, so this should canonicalize various paths when they are
# absolute or relative.
prefixdir = get_option('prefix')
if not prefixdir.startswith('/')
        error('Prefix is not absolute: "@0@"'.format(prefixdir))
endif
if prefixdir != rootprefixdir and rootprefixdir != '/' and not prefixdir.strip('/').startswith(rootprefixdir.strip('/') + '/')
  error('Prefix is not below root prefix (now rootprefix=@0@ prefix=@1@)'.format(
	rootprefixdir, prefixdir))
endif

libdir = join_paths(prefixdir, get_option('libdir'))
rootlibdir = get_option('rootlibdir')
if rootlibdir == ''
  rootlibdir = join_paths(rootprefixdir, get_option('libdir'))
endif
datadir = prefixdir / get_option('datadir')
includedir = prefixdir / get_option('includedir')

pkgconfiglibdir = get_option('pkgconfiglibdir') != '' ? get_option('pkgconfiglibdir') : libdir / 'pkgconfig'

datadir = prefixdir / get_option('datadir')
includedir = prefixdir / get_option('includedir')
sysconfdir =  get_option('sysconfdir')

pkgconfig_script = '''
sed -e s,@VERSION@,@0@,g
    -e s,@prefix@,@1@,g
    -e s,@exec_prefix@,@1@,g
    -e s,@libdir@,@2@,g
    -e s,@includedir@,@3@,g
'''.format(meson.project_version(), prefixdir, libdir, includedir).split()

cc_flags = [
  '-Wall',
  '-Wchar-subscripts',
  '-Wformat-security',
  '-Wmissing-declarations',
  '-Wmissing-prototypes',
  '-Wnested-externs ',
  '-Wshadow',
  '-Wsign-compare',
  '-Wstrict-prototypes',
  '-Wtype-limits',
  '-Wmaybe-uninitialized',
  '-Wdeclaration-after-statement',
  '-Wunused-result',
  '-D_FORTIFY_SOURCE=2',
  '-O2',
]
cc = meson.get_compiler('c')
add_project_arguments(cc.get_supported_arguments(cc_flags), language : 'c')

project_source_root = meson.current_source_dir()

# Remove this after the conversion to meson has been completed
# Cleanup the leftover config.h files to avoid conflicts with the meson
# generated config.h
git = find_program('git', required : false)
env = find_program('env')
if git.found()
  run_command('clean_config.sh',
    env : 'GIT_DIR=@0@/.git'.format(project_source_root),
  )
endif

version_tag = get_option('version-tag')
if version_tag != ''
  vcs_data = configuration_data()
  vcs_data.set('VCS_TAG', version_tag)
  version_h = configure_file(
    configuration : vcs_data,
    input : 'version.h.in',
    output : 'version.h'
  )
else
  vcs_tagger = [
    project_source_root + '/tools/meson-vcs-tag.sh',
    project_source_root,
    meson.project_version()
  ]

  version_h = vcs_tag(
      input : 'version.h.in',
      output : 'version.h',
      command: vcs_tagger
  )
endif

if git.found()
  all_files = run_command(
    env, '-u', 'GIT_WORK_TREE',
    git, '--git-dir=@0@/.git'.format(project_source_root),
         'ls-files', ':/*.[ch]',
    check : false)
  if all_files.returncode() == 0
    all_files = files(all_files.stdout().split())
    custom_target(
            'tags',
            output : 'tags',
            command : [env, 'etags', '-o', '@0@/TAGS'.format(project_source_root)] + all_files)
    run_target(
            'ctags',
            command : [env, 'ctags', '-o', '@0@/tags'.format(project_source_root)] + all_files)
  endif
endif

versiondep = declare_dependency(
  compile_args: ['-include', 'version.h'],
  sources: version_h
)

kmod = dependency('libkmod')
libudev = dependency('libudev')
uuid = dependency('uuid')
json = dependency('json-c')
if get_option('docs').enabled()
  if get_option('asciidoctor').enabled()
    asciidoc = find_program('asciidoctor', required : true)
  else
    asciidoc = find_program('asciidoc', required : true)
    xmlto = find_program('xmlto', required : true)
  endif
endif

if get_option('systemd').enabled()
  systemd = dependency('systemd', required : true)
  systemdunitdir = systemd.get_pkgconfig_variable('systemd_system_unit_dir')
  udev = dependency('udev', required : true)
  udevdir = udev.get_pkgconfig_variable('udevdir')
  udevrulesdir = udevdir / 'rules.d'
endif

cc = meson.get_compiler('c')

# keyutils and iniparser lack pkgconfig
keyutils = cc.find_library('keyutils', required : get_option('keyutils'))
iniparser = cc.find_library('iniparser', required : true)

conf = configuration_data()
check_headers = [
  ['HAVE_DLFCN_H', 'dlfcn.h'],
  ['HAVE_INTTYPES_H', 'inttypes.h'],
  ['HAVE_KEYUTILS_H', 'keyutils.h'],
  ['HAVE_LINUX_VERSION_H', 'linux/version.h'],
  ['HAVE_MEMORY_H', 'memory.h'],
  ['HAVE_STDINT_H', 'stdint.h'],
  ['HAVE_STDLIB_H', 'stdlib.h'],
  ['HAVE_STRINGS_H', 'strings.h'],
  ['HAVE_STRING_H', 'string.h'],
  ['HAVE_SYS_STAT_H', 'sys/stat.h'],
  ['HAVE_SYS_TYPES_H', 'sys/types.h'],
  ['HAVE_UNISTD_H', 'unistd.h'],
]

foreach h : check_headers
  if cc.has_header(h.get(1))
    conf.set(h.get(0), 1)
  endif
endforeach

map_sync_symbols = [
  [ 'signal.h', 'BUS_MCEERR_AR' ],
  [ 'linux/mman.h', 'MAP_SHARED_VALIDATE' ],
  [ 'linux/mman.h', 'MAP_SYNC' ],
]

count = 0
foreach symbol : map_sync_symbols
  if cc.has_header_symbol(symbol[0], symbol[1])
    conf.set('HAVE_DECL_@0@'.format(symbol[1].to_upper()), 1)
    count = count + 1
  endif
endforeach

poison_enabled = false
if get_option('poison').enabled() and count == 3
  poison_enabled = true
endif

conf.set('ENABLE_POISON', poison_enabled)
conf.set('ENABLE_KEYUTILS', get_option('keyutils').enabled())
conf.set('ENABLE_TEST', get_option('test').enabled())
conf.set('ENABLE_DESTRUCTIVE', get_option('destructive').enabled())
conf.set('ENABLE_LOGGING', get_option('logging').enabled())
conf.set('ENABLE_DEBUG', get_option('dbg').enabled())

typeof = cc.run('''
  int main() {
    struct {
      char a[16];
    } x;
    typeof(x) y;

    return sizeof(x) == sizeof(y);
  }
  '''
)

if typeof.compiled() and typeof.returncode() == 1
  conf.set('HAVE_TYPEOF', 1)
  conf.set('HAVE_STATEMENT_EXPR', 1)
endif

if target_machine.endian() == 'big'
  conf.set('HAVE_BIG_ENDIAN', 1)
else
  conf.set('HAVE_LITTLE_ENDIAN', 1)
endif

conf.set('_GNU_SOURCE', true)
conf.set_quoted('PREFIX', get_option('prefix'))
conf.set_quoted('NDCTL_MAN_PATH', get_option('mandir'))

foreach ident : ['secure_getenv', '__secure_getenv']
  conf.set10('HAVE_' + ident.to_upper(), cc.has_function(ident))
endforeach

conf.set10('HAVE_JSON_U64',
  cc.has_function('json_object_new_uint64',
    prefix : '''#include <json-c/json.h>''',
    dependencies : json,
  )
)

ndctlconf_dir = sysconfdir / 'ndctl.conf.d'
ndctlconf = ndctlconf_dir / 'monitor.conf'
conf.set_quoted('NDCTL_CONF_FILE', ndctlconf)
conf.set_quoted('NDCTL_CONF_DIR', ndctlconf_dir)

ndctlkeys_dir = sysconfdir / 'ndctl' / 'keys'
conf.set_quoted('NDCTL_KEYS_DIR', ndctlkeys_dir)

daxctlconf_dir = sysconfdir / 'daxctl.conf.d'
daxctlconf = daxctlconf_dir / 'dax.conf'
conf.set_quoted('DAXCTL_CONF_DIR', daxctlconf_dir)

conf.set_quoted('DAXCTL_MODPROBE_DATA', datadir / 'daxctl/daxctl.conf')
conf.set_quoted('DAXCTL_MODPROBE_INSTALL', sysconfdir / 'modprobe.d/daxctl.conf')

config_h = configure_file(
  input : 'config.h.meson',
  output : 'config.h',
  configuration : conf
)
add_project_arguments('-include', 'config.h', language : 'c')

LIBNDCTL_CURRENT=26
LIBNDCTL_REVISION=0
LIBNDCTL_AGE=20

LIBDAXCTL_CURRENT=6
LIBDAXCTL_REVISION=1
LIBDAXCTL_AGE=5

LIBCXL_CURRENT=2
LIBCXL_REVISION=0
LIBCXL_AGE=1

root_inc = include_directories(['.', 'ndctl', ])

ccan = static_library('ccan',
  [ 'ccan/str/str.c', 'ccan/list/list.c' ],
)
ccan_dep = declare_dependency(link_with : ccan)

subdir('daxctl/lib')
subdir('ndctl/lib')
subdir('cxl/lib')
subdir('util')
subdir('ndctl')
subdir('daxctl')
subdir('cxl')
if get_option('docs').enabled()
  subdir('Documentation/ndctl')
  subdir('Documentation/daxctl')
  subdir('Documentation/cxl')
endif
subdir('test')
subdir('contrib')

# only support spec file generation from git builds
if version_tag == ''
  subdir('rhel')
  subdir('sles')
endif
