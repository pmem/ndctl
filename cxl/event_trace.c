// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2022, Intel Corp. All rights reserved.
#include <stdio.h>
#include <errno.h>
#include <json-c/json.h>
#include <util/json.h>
#include <util/util.h>
#include <util/strbuf.h>
#include <ccan/list/list.h>
#include <uuid/uuid.h>
#include <traceevent/event-parse.h>
#include "event_trace.h"

#define _GNU_SOURCE
#include <string.h>

static struct json_object *num_to_json(void *num, int elem_size, unsigned long flags)
{
	bool sign = flags & TEP_FIELD_IS_SIGNED;
	int64_t val = 0;

	/* special case 64 bit as the call depends on sign */
	if (elem_size == 8) {
		if (sign)
			return json_object_new_int64(*(int64_t *)num);
		else
			return json_object_new_uint64(*(uint64_t *)num);
	}

	/* All others fit in a signed 64 bit */
	switch (elem_size) {
	case 1:
		if (sign)
			val = *(int8_t *)num;
		else
			val = *(uint8_t *)num;
		break;
	case 2:
		if (sign)
			val = *(int16_t *)num;
		else
			val = *(uint16_t *)num;
		break;
	case 4:
		if (sign)
			val = *(int32_t *)num;
		else
			val = *(uint32_t *)num;
		break;
	default:
		/*
		 * Odd sizes are converted in the kernel to one of the above.
		 * It is an error to see them here.
		 */
		return NULL;
	}

	return json_object_new_int64(val);
}

static int cxl_event_to_json(struct tep_event *event, struct tep_record *record,
			     struct list_head *jlist_head)
{
	struct json_object *jevent, *jobj, *jarray;
	struct tep_format_field **fields;
	struct jlist_node *jnode;
	int i, j, rc = 0;

	jnode = malloc(sizeof(*jnode));
	if (!jnode)
		return -ENOMEM;

	jevent = json_object_new_object();
	if (!jevent) {
		rc = -ENOMEM;
		goto err_jnode;
	}
	jnode->jobj = jevent;

	fields = tep_event_fields(event);
	if (!fields) {
		rc = -ENOENT;
		goto err_jevent;
	}

	jobj = json_object_new_string(event->system);
	if (!jobj) {
		rc = -ENOMEM;
		goto err_jevent;
	}
	json_object_object_add(jevent, "system", jobj);

	jobj = json_object_new_string(event->name);
	if (!jobj) {
		rc = -ENOMEM;
		goto err_jevent;
	}
	json_object_object_add(jevent, "event", jobj);

	jobj = json_object_new_uint64(record->ts);
	if (!jobj) {
		rc = -ENOMEM;
		goto err_jevent;
	}
	json_object_object_add(jevent, "timestamp", jobj);

	for (i = 0; fields[i]; i++) {
		struct tep_format_field *f = fields[i];
		int len;

		if (f->flags & TEP_FIELD_IS_STRING) {
			char *str;

			str = tep_get_field_raw(NULL, event, f->name, record, &len, 0);
			if (!str)
				continue;

			jobj = json_object_new_string(str);
			if (!jobj) {
				rc = -ENOMEM;
				goto err_jevent;
			}

			json_object_object_add(jevent, f->name, jobj);
		} else if (f->flags & TEP_FIELD_IS_ARRAY) {
			unsigned char *data;
			int chunks;

			data = tep_get_field_raw(NULL, event, f->name, record, &len, 0);
			if (!data)
				continue;

			jarray = json_object_new_array();
			if (!jarray) {
				rc = -ENOMEM;
				goto err_jevent;
			}

			chunks = f->size / f->elementsize;
			for (j = 0; j < chunks; j++) {
				jobj = num_to_json(data, f->elementsize, f->flags);
				if (!jobj) {
					json_object_put(jarray);
					return -ENOMEM;
				}
				json_object_array_add(jarray, jobj);
				data += f->elementsize;
			}

			json_object_object_add(jevent, f->name, jarray);
		} else { /* single number */
			unsigned char *data;
			char *tmp;

			data = tep_get_field_raw(NULL, event, f->name, record, &len, 0);
			if (!data)
				continue;

			/* check to see if we have a UUID */
			tmp = strcasestr(f->type, "uuid_t");
			if (tmp) {
				char uuid[40];

				uuid_unparse(data, uuid);
				jobj = json_object_new_string(uuid);
				if (!jobj) {
					rc = -ENOMEM;
					goto err_jevent;
				}

				json_object_object_add(jevent, f->name, jobj);
				continue;
			}

			jobj = num_to_json(data, f->elementsize, f->flags);
			if (!jobj) {
				rc = -ENOMEM;
				goto err_jevent;
			}

			json_object_object_add(jevent, f->name, jobj);
		}
	}

	list_add_tail(jlist_head, &jnode->list);
	return 0;

err_jevent:
	json_object_put(jevent);
err_jnode:
	free(jnode);
	return rc;
}
