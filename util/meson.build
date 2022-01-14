util = static_library('util', [
  'parse-options.c',
  'parse-configs.c',
  'usage.c',
  'size.c',
  'main.c',
  'help.c',
  'strbuf.c',
  'wrapper.c',
  'bitmap.c',
  'abspath.c',
  'iomem.c',
  ],
  include_directories : root_inc,
)
util_dep = declare_dependency(link_with : util)
