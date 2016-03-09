AC_PREREQ(2.60)
m4_include([version.m4])
AC_INIT([ndctl],
        GIT_VERSION,
        [linux-nvdimm@lists.01.org],
        [ndctl],
        [https://github.com/pmem/ndctl])
AC_CONFIG_SRCDIR([lib/libndctl.c])
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
AC_CHECK_PROG(ASCIIDOC, [asciidoc], [$(which asciidoc)], [missing])
if test "x$ASCIIDOC" = xmissing; then
       AC_MSG_ERROR([asciidoc needed to build documentation])
fi
AC_SUBST([ASCIIDOC])
AC_CHECK_PROG(XMLTO, [xmlto], [$(which xmlto)], [missing])
if test "x$XMLTO" = xmissing; then
       AC_MSG_ERROR([xmlto needed to build documentation])
fi
AC_SUBST([XMLTO])

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

AC_ARG_ENABLE([docs],
        AS_HELP_STRING([--disable-docs],
	[disable documentation build @<:@default=enabled@:>@]),
        [], enable_docs=yes)
AS_IF([test "x$enable_docs" = "xyes"], [
        AC_DEFINE(ENABLE_DOCS, [1], [Documentation / man pages.])
])
AM_CONDITIONAL([ENABLE_DOCS], [test "x$enable_docs" = "xyes"])

AC_ARG_ENABLE([debug],
        AS_HELP_STRING([--enable-debug], [enable debug messages @<:@default=disabled@:>@]),
        [], [enable_debug=no])
AS_IF([test "x$enable_debug" = "xyes"], [
        AC_DEFINE(ENABLE_DEBUG, [1], [Debug messages.])
])

AC_ARG_ENABLE([test],
        AS_HELP_STRING([--enable-test], [enable ndctl test command @<:@default=disabled@:>@]),
        [], [enable_test=no])
AS_IF([test "x$enable_test" = "xyes"],
	[AC_DEFINE([ENABLE_TEST], [1], [ndctl test support])])
AM_CONDITIONAL([ENABLE_TEST], [test "x$enable_test" = "xyes"])

AC_ARG_ENABLE([destructive],
        AS_HELP_STRING([--enable-destructive], [enable destructive functional tests @<:@default=disabled@:>@]),
        [], [enable_destructive=no])
AS_IF([test "x$enable_destructive" = "xyes"],
	[AC_DEFINE([ENABLE_DESTRUCTIVE], [1], [destructive functional tests support])])
AM_CONDITIONAL([ENABLE_DESTRUCTIVE], [test "x$enable_destructive" = "xyes"])

PKG_CHECK_MODULES([KMOD], [libkmod])
PKG_CHECK_MODULES([UDEV], [libudev])
PKG_CHECK_MODULES([UUID], [uuid])
PKG_CHECK_MODULES([JSON], [json-c])

AC_ARG_ENABLE([local],
        AS_HELP_STRING([--disable-local], [build against kernel ndctl.h @<:@default=system@:>@]),
        [], [enable_local=yes])

AS_IF([test "x$enable_local" = "xyes"], [], [
	AC_CHECK_HEADER([linux/ndctl.h], [
		AC_DEFINE([HAVE_NDCTL_H], [1],
			[Define to 1 if you have <linux/ndctl.h>.])
		], [])
	]
)

# when building against kernel headers check version specific features
AC_MSG_CHECKING([for ARS support])
AC_LANG(C)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
			#ifdef HAVE_NDCTL_H
			#include <linux/ndctl.h>
			#else
			#include "ndctl.h"
			#endif
			]], [[
			int x = ARS_STATUS_MASK;
			]]
		)], [AC_MSG_RESULT([yes])
		     enable_ars=yes
		     AC_DEFINE([HAVE_NDCTL_ARS], [1],
				[Define to 1 if ndctl.h has ARS support.])
		], [AC_MSG_RESULT([no])]
)
AM_CONDITIONAL([ENABLE_ARS], [test "x$enable_ars" = "xyes"])

AC_MSG_CHECKING([for CLEAR ERR support])
AC_LANG(C)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
			#ifdef HAVE_NDCTL_H
			#include <linux/ndctl.h>
			#else
			#include "ndctl.h"
			#endif
			]], [[
			int x = ND_CMD_CLEAR_ERROR;
			]]
		)], [AC_MSG_RESULT([yes])
		     enable_clear_err=yes
		     AC_DEFINE([HAVE_NDCTL_CLEAR_ERROR], [1],
				[Define to 1 if ndctl.h has CLEAR ERR support.])
		], [AC_MSG_RESULT([no])]
)
AM_CONDITIONAL([ENABLE_CLEAR_ERROR], [test "x$enable_clear_err" = "xyes"])

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
"
AC_SUBST([my_CFLAGS])

AC_CONFIG_HEADERS(config.h)
AC_CONFIG_FILES([
        Makefile
        Documentation/Makefile
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
