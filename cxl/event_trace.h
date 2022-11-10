/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2022 Intel Corporation. All rights reserved. */
#ifndef __CXL_EVENT_TRACE_H__
#define __CXL_EVENT_TRACE_H__

#include <json-c/json.h>
#include <ccan/list/list.h>

struct jlist_node {
	struct json_object *jobj;
	struct list_node list;
};

#endif
