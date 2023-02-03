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

struct event_ctx {
	const char *system;
	struct list_head jlist_head;
	const char *event_name; /* optional */
	int (*parse_event)(struct tep_event *event, struct tep_record *record,
			   struct list_head *jlist_head); /* optional */
};

int cxl_parse_events(struct tracefs_instance *inst, struct event_ctx *ectx);
int cxl_event_tracing_enable(struct tracefs_instance *inst, const char *system,
		const char *event);
int cxl_event_tracing_disable(struct tracefs_instance *inst);

#endif
