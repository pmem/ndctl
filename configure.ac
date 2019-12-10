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
	[], enable_asciidoctor=yes)
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

AC_CHECK_DECLS([BUS_MCEERR_AR], [enable_bus_mc_err=yes], [], [[#include <signal.h>]])
AC_CHECK_DECLS([MAP_SHARED_VALIDATE], [kernel_map_shared_validate=yes], [], [[#include <linux/mman.h>]])
AC_CHECK_DECLS([MAP_SYNC], [kernel_map_sync=yes], [], [[#include <linux/mman.h>]])
AS_UNSET([ac_cv_have_decl_MAP_SHARED_VALIDATE])
AS_UNSET([ac_cv_have_decl_MAP_SYNC])
AC_CHECK_DECLS([MAP_SHARED_VALIDATE], [enable_map_shared_validate=yes], [], [[#include <sys/mman.h>]])
AC_CHECK_DECLS([MAP_SYNC], [enable_map_sync=yes], [], [[#include <sys/mman.h>]])

if test "x$kernel_map_shared_validate" = "xyes" -a "x$enable_map_shared_validate" != "xyes" ; then
	AC_MSG_WARN([MAP_SHARED_VALIDATE supported by kernel but not by <sys/mman.h>, consider installing glibc-2.28 or later.])
fi
if test "x$kernel_map_shared_validate" != "xyes" -a "x$enable_map_shared_validate" != "xyes" ; then
	AC_MSG_WARN([MAP_SHARED_VALIDATE not supported by kernel, consider installing kernel-4.15 or later.])
fi
if test "x$kernel_map_sync" = "xyes" -a "x$enable_map_sync" != "xyes" ; then
	AC_MSG_WARN([MAP_SYNC supported by kernel but not by <sys/mman.h>, consider installing glibc-2.28 or later.])
fi
if test "x$kernel_map_sync" != "xyes" -a "x$enable_map_sync" != "xyes" ; then
	AC_MSG_WARN([MAP_SYNC not supported by kernel or architecture, consider installing kernel-4.15 or later.])
fi

AS_IF([test "x$enable_bus_mc_err" = "xyes" -a "x$enable_map_sync" = "xyes" -a "x$enable_map_shared_validate" = "xyes"],
	[AC_DEFINE([ENABLE_POISON], [1], [ndctl test poison support])])
AM_CONDITIONAL([ENABLE_POISON],
	[test "x$enable_bus_mc_err" = "xyes" -a "x$enable_map_sync" = "xyes" -a "x$enable_map_shared_validate" = "xyes"])

PKG_CHECK_MODULES([KMOD], [libkmod])
PKG_CHECK_MODULES([UDEV], [libudev])
PKG_CHECK_MODULES([UUID], [uuid],
	[AC_DEFINE([HAVE_UUID], [1], [Define to 1 if using libuuid])])
PKG_CHECK_MODULES([JSON], [json-c])

AC_ARG_WITH([bash],
	AS_HELP_STRING([--with-bash],
		[Enable bash auto-completion. @<:@default=yes@:>@]),
	[],
	[with_bash=yes])

if test "x$with_bash" = "xyes"; then
	PKG_CHECK_MODULES([BASH_COMPLETION], [bash-completion >= 2.0],
		[BASH_COMPLETION_DIR=$($PKG_CONFIG --variable=completionsdir bash-completion)], [])
fi

AC_SUBST([BASH_COMPLETION_DIR])
AM_CONDITIONAL([ENABLE_BASH_COMPLETION], [test "x$with_bash" = "xyes"])

AC_ARG_ENABLE([local],
        AS_HELP_STRING([--disable-local], [build against kernel ndctl.h @<:@default=system@:>@]),
        [], [enable_local=yes])

AC_CHECK_HEADERS_ONCE([linux/version.h])

AC_CHECK_FUNCS([ \
	__secure_getenv \
	secure_getenv\
])

AC_ARG_WITH([systemd],
	AS_HELP_STRING([--with-systemd],
		[Enable systemd functionality (monitor). @<:@default=yes@:>@]),
	[], [with_systemd=yes])

if test "x$with_systemd" = "xyes"; then
	PKG_CHECK_MODULES([SYSTEMD], [systemd],
	[systemd_unitdir=$($PKG_CONFIG --variable=systemdsystemunitdir systemd)], [])
fi

AC_SUBST([systemd_unitdir])
AM_CONDITIONAL([ENABLE_SYSTEMD_UNITS], [test "x$with_systemd" = "xyes"])

ndctl_monitorconfdir=${sysconfdir}/ndctl
ndctl_monitorconf=monitor.conf
AC_SUBST([ndctl_monitorconfdir])
AC_SUBST([ndctl_monitorconf])

daxctl_modprobe_datadir=${datadir}/daxctl
daxctl_modprobe_data=daxctl.conf
AC_SUBST([daxctl_modprobe_datadir])
AC_SUBST([daxctl_modprobe_data])

AC_ARG_WITH([keyutils],
	    AS_HELP_STRING([--with-keyutils],
			[Enable keyutils functionality (security).  @<:@default=yes@:>@]), [], [with_keyutils=yes])

if test "x$with_keyutils" = "xyes"; then
	AC_CHECK_HEADERS([keyutils.h],,[
		AC_MSG_ERROR([keyutils.h not found, consider installing the keyutils library development package (variously named keyutils-libs-devel, keyutils-devel, or libkeyutils-dev).])
		])
fi
AS_IF([test "x$with_keyutils" = "xyes"],
	[AC_DEFINE([ENABLE_KEYUTILS], [1], [Enable keyutils support])])
AM_CONDITIONAL([ENABLE_KEYUTILS], [test "x$with_keyutils" = "xyes"])

ndctl_keysdir=${sysconfdir}/ndctl/keys
ndctl_keysreadme=keys.readme
AC_SUBST([ndctl_keysdir])
AC_SUBST([ndctl_keysreadme])

my_CFLAGS="\
-Wall \
-Wchar-subscripts \
-Wformat-security \
-Wmissing-declarations \
-Wmissing-prototypes \
-Wnested-externs \
-Wshadow \
-Wsign-compare \
-Wstrict-prototypes \
-Wtype-limits \
-Wmaybe-uninitialized \
-Wdeclaration-after-statement \
-Wunused-result \
-D_FORTIFY_SOURCE=2 \
-O2
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
