/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2005 Junio C Hamano. All rights reserved. */
/* Copyright (C) 2005 Linus Torvalds. All rights reserved. */

/* originally copied from perf and git */

#ifndef __UTIL_H__
#define __UTIL_H__
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#pragma GCC diagnostic ignored "-Wmissing-prototypes"

#ifdef __GNUC__
#define NORETURN __attribute__((__noreturn__))
#else
#define NORETURN
#ifndef __attribute__
#define __attribute__(x)
#endif
#endif

#ifndef __maybe_unused
# define __maybe_unused         /* unimplemented */
#endif

#define is_dir_sep(c) ((c) == '/')

#define alloc_nr(x) (((x)+16)*3/2)

#define __round_mask(x, y) ((__typeof__(x))((y)-1))
#define round_up(x, y) ((((x)-1) | __round_mask(x, y))+1)
#define round_down(x, y) ((x) & ~__round_mask(x, y))

#define rounddown(x, y) (				\
{							\
	typeof(x) __x = (x);				\
	__x - (__x % (y));				\
}							\
)

/*
 * Realloc the buffer pointed at by variable 'x' so that it can hold
 * at least 'nr' entries; the number of entries currently allocated
 * is 'alloc', using the standard growing factor alloc_nr() macro.
 *
 *  DO NOT USE any expression with side-effect for 'x' or 'alloc'.
 */
#define ALLOC_GROW(x, nr, alloc) \
        do { \
                if ((nr) > alloc) { \
                        if (alloc_nr(alloc) < (nr)) \
                                alloc = (nr); \
                        else \
                                alloc = alloc_nr(alloc); \
                        x = xrealloc((x), alloc * sizeof(*(x))); \
                } \
        } while(0)

#define zfree(ptr) ({ free(*ptr); *ptr = NULL; })

#define BUILD_BUG_ON_ZERO(e) (sizeof(struct { int:-!!(e); }))
#define BUILD_BUG_ON(condition) ((void)sizeof(char[1 - 2*!!(condition)]))

enum {
	READ, WRITE,
};

static inline const char *skip_prefix(const char *str, const char *prefix)
{
        size_t len = strlen(prefix);
        return strncmp(str, prefix, len) ? NULL : str + len;
}

static inline int is_absolute_path(const char *path)
{
	return path[0] == '/';
}

void usage(const char *err) NORETURN;
void die(const char *err, ...) NORETURN __attribute__((format (printf, 1, 2)));
int error(const char *err, ...) __attribute__((format (printf, 1, 2)));
void warning(const char *err, ...) __attribute__((format (printf, 1, 2)));
void set_die_routine(void (*routine)(const char *err, va_list params) NORETURN);
char *xstrdup(const char *str);
void *xrealloc(void *ptr, size_t size);
int prefixcmp(const char *str, const char *prefix);
char *prefix_filename(const char *pfx, const char *arg);
void fix_filename(const char *prefix, const char **file);

#endif /* __UTIL_H__ */
