AC_PREREQ(2.60)
AC_INIT([ndctl],
        [38],
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

AC_ARG_ENABLE([test],
        AS_HELP_STRING([--enable-test], [enable ndctl test command @<:@default=disabled@:>@]),
        [], [enable_test=no])
AS_IF([test "x$enable_test" = "xyes"],
	[AC_DEFINE([ENABLE_TEST], [1], [ndctl test support])])

AC_ARG_WITH([udev], [AS_HELP_STRING([--without-udev], [disable support for udev])],
			[], [with_udev=yes])

LIBUDEV=
AS_IF([test "x$with_udev" != "xno"],
	[AC_CHECK_LIB([udev], [main],
		[AC_SUBST([LIBUDEV], ["-ludev"])
			AC_DEFINE([HAVE_LIBUDEV], [1], [Define if you have libudev])
		],
		[AC_MSG_FAILURE([udev test failed (--without-udev to disable)])], [])
	]
)

AC_ARG_WITH([kmod], [AS_HELP_STRING([--without-kmod], [disable support for kmod])],
			[], [with_kmod=yes])

LIBKMOD=
AS_IF([test "x$with_kmod" != "xno"],
	[AC_CHECK_LIB([kmod], [main],
		[AC_SUBST([LIBKMOD], ["-lkmod"])
			AC_DEFINE([HAVE_LIBKMOD], [1], [Define if you have libkmod])
		],
		[AC_MSG_FAILURE([kmod test failed (--without-kmod to disable)])], [])
	]
)

AC_ARG_WITH([uuid], [AS_HELP_STRING([--without-uuid], [disable support for uuid])],
			[], [with_uuid=yes])

LIBUUID=
AS_IF([test "x$with_uuid" != "xno"],
	[AC_CHECK_LIB([uuid], [main],
		[AC_SUBST([LIBUUID], ["-luuid"])
			AC_DEFINE([HAVE_LIBUUID], [1], [Define if you have libuuid])
		],
		[AC_MSG_FAILURE([uuid test failed (--without-uuid to disable)])], [])
	]
)

AC_CHECK_HEADER([linux/ndctl.h],
		[AC_DEFINE([HAVE_NDCTL_H], [1],
			[Define to 1 if you have <linux/ndctl.h>.])],
		[])


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
