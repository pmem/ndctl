/* SPDX-License-Identifier: LGPL-2.1 */
/* (C) Copyright IBM 2020 */

#ifndef __PAPR_H__
#define __PAPR_H__

#include <papr_pdsm.h>

/* Wraps a nd_cmd generic header with pdsm header */
struct nd_pkg_papr {
	struct nd_cmd_pkg gen;
	struct nd_pkg_pdsm pdsm;
};

#endif /* __PAPR_H__ */
