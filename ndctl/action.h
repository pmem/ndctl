/*
 * Copyright(c) 2015-2017 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0
 */
#ifndef __NDCTL_ACTION_H__
#define __NDCTL_ACTION_H__
enum device_action {
	ACTION_ENABLE,
	ACTION_DISABLE,
	ACTION_CREATE,
	ACTION_DESTROY,
	ACTION_CHECK,
	ACTION_WAIT,
	ACTION_START,
	ACTION_CLEAR,
	ACTION_READ_INFOBLOCK,
};
#endif /* __NDCTL_ACTION_H__ */
