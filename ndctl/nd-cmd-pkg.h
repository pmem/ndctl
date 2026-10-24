/* SPDX-License-Identifier: LGPL-2.1 */
/* Copyright (C) 2026 Intel Corporation. All rights reserved. */
#ifndef __NDCTL_ND_CMD_PKG_H__
#define __NDCTL_ND_CMD_PKG_H__

#include <stddef.h>
#include <linux/types.h>
#include <ndctl/ndctl.h>

/*
 * struct nd_cmd_pkg ends in a flexible array, so it cannot be embedded
 * ahead of a typed payload in ISO C. This copy of the fixed header
 * can be. It must match the kernel uapi definition of struct nd_cmd_pkg,
 * which the assertions below check against ndctl/ndctl.h.
 */
struct nd_cmd_pkg_hdr {
	__u64   nd_family;
	__u64   nd_command;
	__u32   nd_size_in;
	__u32   nd_size_out;
	__u32   nd_reserved2[9];
	__u32   nd_fw_size;
};

#define ND_CMD_PKG_HDR_ASSERT_OFFSET(m)					\
	_Static_assert(offsetof(struct nd_cmd_pkg_hdr, m) ==		\
		       offsetof(struct nd_cmd_pkg, m),			\
		       "nd_cmd_pkg_hdr." #m " offset mismatch")

ND_CMD_PKG_HDR_ASSERT_OFFSET(nd_family);
ND_CMD_PKG_HDR_ASSERT_OFFSET(nd_command);
ND_CMD_PKG_HDR_ASSERT_OFFSET(nd_size_in);
ND_CMD_PKG_HDR_ASSERT_OFFSET(nd_size_out);
ND_CMD_PKG_HDR_ASSERT_OFFSET(nd_reserved2);
ND_CMD_PKG_HDR_ASSERT_OFFSET(nd_fw_size);

#undef ND_CMD_PKG_HDR_ASSERT_OFFSET

_Static_assert(sizeof(struct nd_cmd_pkg_hdr) ==
	       offsetof(struct nd_cmd_pkg, nd_payload),
	       "nd_cmd_pkg_hdr size mismatch");
_Static_assert(sizeof(struct nd_cmd_pkg_hdr) == sizeof(struct nd_cmd_pkg),
	       "nd_cmd_pkg_hdr size mismatch");
_Static_assert(_Alignof(struct nd_cmd_pkg_hdr) ==
	       _Alignof(struct nd_cmd_pkg),
	       "nd_cmd_pkg_hdr alignment mismatch");

#endif /* __NDCTL_ND_CMD_PKG_H__ */
