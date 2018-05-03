AC_PREREQ(2.60)
m4_include([version.m4])
AC_INIT([ndctl],
        GIT_VERSION,
        [linux-nvdimm@lists.01.org],
        [ndctl],
        [https://github.com/pmem/ndctl])
AC_CONFIG_SRCDIR([ndctl/lib/libndctl.c])
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE([
	check-news
	foreign
	1.11
	-Wall
	-Wno-portability
	silent-rules
	tar-pax
	no-dist-gzip
	dist-xz
	subdir-objects
])
AC_PROG_CC_STDC
AC_USE_SYSTEM_EXTENSIONS
AC_SYS_LARGEFILE
AC_CONFIG_MACRO_DIR([m4])
AM_SILENT_RULES([yes])
LT_INIT([
	disable-static
	pic-only
])
AC_PREFIX_DEFAULT([/usr])

AC_PROG_SED
AC_PROG_MKDIR_P

AC_ARG_ENABLE([docs],
        AS_HELP_STRING([--disable-docs],
	[disable documentation build @<:@default=enabled@:>@]),
        [], enable_docs=yes)
AS_IF([test "x$enable_docs" = "xyes"], [
        AC_DEFINE(ENABLE_DOCS, [1], [Documentation / man pages.])
])
AM_CONDITIONAL([ENABLE_DOCS], [test "x$enable_docs" = "xyes"])

AC_ARG_ENABLE([asciidoctor],
	AS_HELP_STRING([--enable-asciidoctor],
	[use asciidoctor for documentation build]),
	[], enable_asciidoctor=no)
AM_CONDITIONAL([USE_ASCIIDOCTOR], [test "x$enable_asciidoctor" = "xyes"])
if test "x$enable_asciidoctor" = "xyes"; then
	asciidoc="asciidoctor"
else
	asciidoc="asciidoc"
fi
AC_CHECK_PROG(ASCIIDOC, [$asciidoc], [$(which $asciidoc)], [missing])
if test "x$ASCIIDOC" = xmissing -a "x$enable_docs" = "xyes"; then
	AC_MSG_ERROR([$asciidoc needed to build documentation])
fi
AC_SUBST([ASCIIDOC])

if test x"$asciidoc" = x"asciidoc"; then
AC_CHECK_PROG(XMLTO, [xmlto], [$(which xmlto)], [missing])
if test "x$XMLTO" = xmissing -a "x$enable_docs" = "xyes"; then
       AC_MSG_ERROR([xmlto needed to build documentation])
fi
AC_SUBST([XMLTO])
fi

AC_C_TYPEOF
AC_DEFINE([HAVE_STATEMENT_EXPR], 1, [Define to 1 if you have statement expressions.])

AC_C_BIGENDIAN(
	AC_DEFINE(HAVE_BIG_ENDIAN, 1, [Define to 1 if big-endian-arch]),
	AC_DEFINE(HAVE_LITTLE_ENDIAN, 1, [Define to 1 if little-endian-arch]),
	[], [])

AC_ARG_ENABLE([logging],
        AS_HELP_STRING([--disable-logging], [disable system logging @<:@default=enabled@:>@]),
        [], enable_logging=yes)
AS_IF([test "x$enable_logging" = "xyes"], [
        AC_DEFINE(ENABLE_LOGGING, [1], [System logging.])
])

AC_ARG_ENABLE([debug],
        AS_HELP_STRING([--enable-debug], [enable debug messages @<:@default=disabled@:>@]),
        [], [enable_debug=no])
AS_IF([test "x$enable_debug" = "xyes"], [
        AC_DEFINE(ENABLE_DEBUG, [1], [Debug messages.])
])

AC_ARG_ENABLE([destructive],
        AS_HELP_STRING([--enable-destructive], [enable destructive functional tests @<:@default=disabled@:>@]),
        [], [enable_destructive=no])
AS_IF([test "x$enable_destructive" = "xyes"],
	[AC_DEFINE([ENABLE_DESTRUCTIVE], [1], [destructive functional tests support])])
AM_CONDITIONAL([ENABLE_DESTRUCTIVE], [test "x$enable_destructive" = "xyes"])

AC_ARG_ENABLE([test],
        AS_HELP_STRING([--enable-test], [enable ndctl test command @<:@default=disabled@:>@]),
        [], [enable_test=$enable_destructive])
AS_IF([test "x$enable_test" = "xyes"],
	[AC_DEFINE([ENABLE_TEST], [1], [ndctl test support])])
AM_CONDITIONAL([ENABLE_TEST], [test "x$enable_test" = "xyes"])

PKG_CHECK_MODULES([KMOD], [libkmod])
PKG_CHECK_MODULES([UDEV], [libudev])
PKG_CHECK_MODULES([UUID], [uuid])
PKG_CHECK_MODULES([JSON], [json-c])

AC_ARG_WITH([bash-completion-dir],
	AS_HELP_STRING([--with-bash-completion-dir[=PATH]],
		[Install the bash auto-completion script in this directory. @<:@default=yes@:>@]),
	[],
	[with_bash_completion_dir=yes])

if test "x$with_bash_completion_dir" = "xyes"; then
	PKG_CHECK_MODULES([BASH_COMPLETION], [bash-completion >= 2.0],
		[BASH_COMPLETION_DIR="`pkg-config --variable=completionsdir bash-completion`"],
		[BASH_COMPLETION_DIR="$datadir/bash-completion/completions"])
else
	BASH_COMPLETION_DIR="$with_bash_completion_dir"
fi

AC_SUBST([BASH_COMPLETION_DIR])
AM_CONDITIONAL([ENABLE_BASH_COMPLETION],[test "x$with_bash_completion_dir" != "xno"])

AC_ARG_ENABLE([local],
        AS_HELP_STRING([--disable-local], [build against kernel ndctl.h @<:@default=system@:>@]),
        [], [enable_local=yes])

AC_CHECK_HEADERS_ONCE([linux/version.h])

AC_CHECK_FUNCS([ \
	__secure_getenv \
	secure_getenv\
])

my_CFLAGS="\
-Wall \
-Wchar-subscripts \
-Wformat-security \
-Wmissing-declarations \
-Wmissing-prototypes \
-Wnested-externs \
-Wpointer-arith \
-Wshadow \
-Wsign-compare \
-Wstrict-prototypes \
-Wtype-limits \
-Wmaybe-uninitialized \
-Wunused-result \
-D_FORTIFY_SOURCE=2
"
AC_SUBST([my_CFLAGS])

AC_CONFIG_HEADERS(config.h)
AC_CONFIG_FILES([
        Makefile
        daxctl/lib/Makefile
        ndctl/lib/Makefile
        ndctl/Makefile
        daxctl/Makefile
        test/Makefile
        Documentation/ndctl/Makefile
        Documentation/daxctl/Makefile
])

AC_OUTPUT
AC_MSG_RESULT([
        $PACKAGE $VERSION
        =====

        prefix:                 ${prefix}
        sysconfdir:             ${sysconfdir}
        libdir:                 ${libdir}
        includedir:             ${includedir}

        compiler:               ${CC}
        cflags:                 ${CFLAGS}
        ldflags:                ${LDFLAGS}

        logging:                ${enable_logging}
        debug:                  ${enable_debug}
])
