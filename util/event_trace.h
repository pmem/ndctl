/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2022 Intel Corporation. All rights reserved. */
#ifndef __UTIL_EVENT_TRACE_H__
#define __UTIL_EVENT_TRACE_H__

#include <json-c/json.h>
#include <ccan/list/list.h>
#include <ccan/short_types/short_types.h>

struct jlist_node {
	struct json_object *jobj;
	struct list_node list;
};

struct event_ctx {
	const char *system;
	struct list_head jlist_head;
	const char *event_name; /* optional */
	int event_pid; /* optional */
	int (*parse_event)(struct tep_event *event, struct tep_record *record,
			   struct event_ctx *ctx);
};

int trace_event_parse(struct tracefs_instance *inst, struct event_ctx *ectx);
int trace_event_enable(struct tracefs_instance *inst, const char *system,
		       const char *event);
int trace_event_disable(struct tracefs_instance *inst);
u8 trace_get_field_u8(struct tep_event *event, struct tep_record *record,
		      const char *name);
u32 trace_get_field_u32(struct tep_event *event, struct tep_record *record,
			const char *name);
u64 trace_get_field_u64(struct tep_event *event, struct tep_record *record,
			const char *name);
#endif
