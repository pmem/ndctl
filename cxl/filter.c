// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2015-2022 Intel Corporation. All rights reserved.
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <util/log.h>
#include <util/json.h>
#include <cxl/libcxl.h>
#include <json-c/json.h>

#include "filter.h"
#include "json.h"

static const char *which_sep(const char *filter)
{
	if (strchr(filter, ' '))
		return " ";
	if (strchr(filter, ','))
		return ",";
	return " ";
}

bool cxl_filter_has(const char *__filter, const char *needle)
{
	char *filter, *save;
	const char *arg;

	if (!needle)
		return true;

	if (!__filter)
		return false;

	filter = strdup(__filter);
	if (!filter)
		return false;

	for (arg = strtok_r(filter, which_sep(__filter), &save); arg;
	     arg = strtok_r(NULL, which_sep(__filter), &save))
		if (strstr(arg, needle))
			break;

	free(filter);
	if (arg)
		return true;
	return false;
}

static struct cxl_endpoint *
util_cxl_endpoint_filter(struct cxl_endpoint *endpoint, const char *__ident)
{
	char *ident, *save;
	const char *arg;
	int endpoint_id;

	if (!__ident)
		return endpoint;

	ident = strdup(__ident);
	if (!ident)
		return NULL;

	for (arg = strtok_r(ident, which_sep(__ident), &save); arg;
	     arg = strtok_r(NULL, which_sep(__ident), &save)) {
		if (strcmp(arg, "all") == 0)
			break;

		if ((sscanf(arg, "%d", &endpoint_id) == 1 ||
		     sscanf(arg, "endpoint%d", &endpoint_id) == 1) &&
		    cxl_endpoint_get_id(endpoint) == endpoint_id)
			break;

		if (strcmp(arg, cxl_endpoint_get_devname(endpoint)) == 0)
			break;
	}

	free(ident);
	if (arg)
		return endpoint;
	return NULL;
}

static struct cxl_port *__util_cxl_port_filter(struct cxl_port *port,
					       const char *__ident)
{
	char *ident, *save;
	const char *arg;
	int port_id;

	if (!__ident)
		return port;

	ident = strdup(__ident);
	if (!ident)
		return NULL;

	for (arg = strtok_r(ident, which_sep(__ident), &save); arg;
	     arg = strtok_r(NULL, which_sep(__ident), &save)) {
		if (strcmp(arg, "all") == 0)
			break;

		if (strcmp(arg, "root") == 0 && cxl_port_is_root(port))
			break;

		if (strcmp(arg, "switch") == 0 && cxl_port_is_switch(port))
			break;

		if (strcmp(arg, "endpoint") == 0 && cxl_port_is_endpoint(port))
			break;

		if ((sscanf(arg, "%d", &port_id) == 1 ||
		     sscanf(arg, "port%d", &port_id) == 1) &&
		    cxl_port_get_id(port) == port_id)
			break;

		if (strcmp(arg, cxl_port_get_devname(port)) == 0)
			break;
	}

	free(ident);
	if (arg)
		return port;
	return NULL;
}

enum cxl_port_filter_mode {
	CXL_PF_SINGLE,
	CXL_PF_ANCESTRY,
};

static enum cxl_port_filter_mode pf_mode(struct cxl_filter_params *p)
{
	if (p->single)
		return CXL_PF_SINGLE;
	return CXL_PF_ANCESTRY;
}

static struct cxl_port *util_cxl_port_filter(struct cxl_port *port,
					     const char *ident,
					     enum cxl_port_filter_mode mode)
{
	struct cxl_port *iter = port;

	while (iter) {
		if (__util_cxl_port_filter(iter, ident))
			return port;
		if (mode == CXL_PF_SINGLE)
			return NULL;
		iter = cxl_port_get_parent(iter);
	}

	return NULL;
}

static struct cxl_endpoint *
util_cxl_endpoint_filter_by_port(struct cxl_endpoint *endpoint,
				 const char *ident,
				 enum cxl_port_filter_mode mode)
{
	struct cxl_port *iter = cxl_endpoint_get_port(endpoint);

	if (util_cxl_port_filter(iter, ident, CXL_PF_SINGLE))
		return endpoint;
	iter = cxl_port_get_parent(iter);
	if (!iter)
		return NULL;
	if (util_cxl_port_filter(iter, ident, mode))
		return endpoint;

	return NULL;
}

static struct cxl_decoder *
util_cxl_decoder_filter_by_port(struct cxl_decoder *decoder, const char *ident,
				enum cxl_port_filter_mode mode)
{
	struct cxl_port *port = cxl_decoder_get_port(decoder);

	if (util_cxl_port_filter(port, ident, mode))
		return decoder;
	return NULL;
}

static struct cxl_bus *util_cxl_bus_filter(struct cxl_bus *bus,
					   const char *__ident)
{
	char *ident, *save;
	const char *arg;
	int bus_id;

	if (!__ident)
		return bus;

	ident = strdup(__ident);
	if (!ident)
		return NULL;

	for (arg = strtok_r(ident, which_sep(__ident), &save); arg;
	     arg = strtok_r(NULL, which_sep(__ident), &save)) {
		if (strcmp(arg, "all") == 0)
			break;

		if ((sscanf(arg, "%d", &bus_id) == 1 ||
		     sscanf(arg, "root%d", &bus_id) == 1) &&
		    cxl_bus_get_id(bus) == bus_id)
			break;

		if (strcmp(arg, cxl_bus_get_devname(bus)) == 0)
			break;

		if (strcmp(arg, cxl_bus_get_provider(bus)) == 0)
			break;
	}

	free(ident);
	if (arg)
		return bus;
	return NULL;
}

static struct cxl_port *util_cxl_port_filter_by_bus(struct cxl_port *port,
						    const char *__ident)
{
	struct cxl_ctx *ctx = cxl_port_get_ctx(port);
	struct cxl_bus *bus;

	if (!__ident)
		return port;

	if (cxl_port_is_root(port)) {
		bus = cxl_port_to_bus(port);
		bus = util_cxl_bus_filter(bus, __ident);
		return bus ? port : NULL;
	}

	cxl_bus_foreach(ctx, bus) {
		if (!util_cxl_bus_filter(bus, __ident))
			continue;
		if (bus == cxl_port_get_bus(port))
			return port;
	}

	return NULL;
}

static struct cxl_decoder *
util_cxl_decoder_filter_by_bus(struct cxl_decoder *decoder, const char *__ident)
{
	struct cxl_port *port = cxl_decoder_get_port(decoder);

	if (!util_cxl_port_filter_by_bus(port, __ident))
		return NULL;
	return decoder;
}

static struct cxl_memdev *
util_cxl_memdev_serial_filter(struct cxl_memdev *memdev, const char *__serials)
{
	unsigned long long serial = 0;
	char *serials, *save, *end;
	const char *arg;

	if (!__serials)
		return memdev;

	serials = strdup(__serials);
	if (!serials)
		return NULL;

	for (arg = strtok_r(serials, which_sep(__serials), &save); arg;
	     arg = strtok_r(NULL, which_sep(__serials), &save)) {
		serial = strtoull(arg, &end, 0);
		if (!arg[0] || end[0] != 0)
			continue;
		if (cxl_memdev_get_serial(memdev) == serial)
			break;
	}

	free(serials);
	if (arg)
		return memdev;
	return NULL;
}

struct cxl_memdev *util_cxl_memdev_filter(struct cxl_memdev *memdev,
					  const char *__ident,
					  const char *serials)
{
	char *ident, *save;
	const char *name;
	int memdev_id;

	if (!__ident)
		return util_cxl_memdev_serial_filter(memdev, serials);

	ident = strdup(__ident);
	if (!ident)
		return NULL;

	for (name = strtok_r(ident, which_sep(__ident), &save); name;
	     name = strtok_r(NULL, which_sep(__ident), &save)) {
		if (strcmp(name, "all") == 0)
			break;

		if ((sscanf(name, "%d", &memdev_id) == 1 ||
		     sscanf(name, "mem%d", &memdev_id) == 1) &&
		    cxl_memdev_get_id(memdev) == memdev_id)
			break;

		if (strcmp(name, cxl_memdev_get_devname(memdev)) == 0)
			break;
	}

	free(ident);
	if (name)
		return util_cxl_memdev_serial_filter(memdev, serials);
	return NULL;
}

static struct cxl_bus *util_cxl_bus_filter_by_memdev(struct cxl_bus *bus,
						     const char *ident,
						     const char *serial)
{
	struct cxl_ctx *ctx = cxl_bus_get_ctx(bus);
	struct cxl_memdev *memdev;

	if (!ident && !serial)
		return bus;

	cxl_memdev_foreach(ctx, memdev) {
		if (!util_cxl_memdev_filter(memdev, ident, serial))
			continue;
		if (cxl_memdev_get_bus(memdev) == bus)
			return bus;
	}

	return NULL;
}

static struct cxl_endpoint *
util_cxl_endpoint_filter_by_memdev(struct cxl_endpoint *endpoint,
				   const char *ident, const char *serial)
{
	struct cxl_ctx *ctx = cxl_endpoint_get_ctx(endpoint);
	struct cxl_memdev *memdev;

	if (!ident && !serial)
		return endpoint;

	cxl_memdev_foreach(ctx, memdev) {
		if (!util_cxl_memdev_filter(memdev, ident, serial))
			continue;
		if (cxl_memdev_get_endpoint(memdev) == endpoint)
			return endpoint;
	}

	return NULL;
}

static struct cxl_port *util_cxl_port_filter_by_memdev(struct cxl_port *port,
						       const char *ident,
						       const char *serial)
{
	struct cxl_ctx *ctx = cxl_port_get_ctx(port);
	struct cxl_memdev *memdev;

	if (!ident && !serial)
		return port;

	cxl_memdev_foreach(ctx, memdev) {
		if (!util_cxl_memdev_filter(memdev, ident, serial))
			continue;
		if (cxl_port_hosts_memdev(port, memdev))
			return port;
	}

	return NULL;
}

static struct cxl_decoder *util_cxl_decoder_filter(struct cxl_decoder *decoder,
						   const char *__ident)
{
	struct cxl_port *port = cxl_decoder_get_port(decoder);
	int pid, did;
	char *ident, *save;
	const char *name;

	if (!__ident)
		return decoder;

	ident = strdup(__ident);
	if (!ident)
		return NULL;

	for (name = strtok_r(ident, which_sep(__ident), &save); name;
	     name = strtok_r(NULL, which_sep(__ident), &save)) {
		if (strcmp(name, "all") == 0)
			break;

		if (strcmp(name, "root") == 0 && cxl_port_is_root(port))
			break;
		if (strcmp(name, "switch") == 0 && cxl_port_is_switch(port))
			break;
		if (strcmp(name, "endpoint") == 0 && cxl_port_is_endpoint(port))
			break;

		if ((sscanf(name, "%d.%d", &pid, &did) == 2 ||
		     sscanf(name, "decoder%d.%d", &pid, &did) == 2) &&
		    cxl_port_get_id(port) == pid &&
		    cxl_decoder_get_id(decoder) == did)
			break;

		if (strcmp(name, cxl_decoder_get_devname(decoder)) == 0)
			break;
	}

	free(ident);
	if (name)
		return decoder;
	return NULL;
}

static struct cxl_decoder *
util_cxl_decoder_filter_by_memdev(struct cxl_decoder *decoder,
				  const char *ident, const char *serial)
{
	struct cxl_ctx *ctx = cxl_decoder_get_ctx(decoder);
	struct cxl_memdev *memdev;

	if (!ident && !serial)
		return decoder;

	cxl_memdev_foreach(ctx, memdev) {
		if (!util_cxl_memdev_filter(memdev, ident, serial))
			continue;
		if (cxl_decoder_get_target_by_memdev(decoder, memdev))
			return decoder;
	}

	return NULL;
}

static bool __memdev_filter_by_decoder(struct cxl_memdev *memdev,
				       struct cxl_port *port, const char *ident)
{
	struct cxl_decoder *decoder;
	struct cxl_endpoint *endpoint;

	cxl_decoder_foreach(port, decoder) {
		if (!util_cxl_decoder_filter(decoder, ident))
			continue;
		if (cxl_decoder_get_target_by_memdev(decoder, memdev))
			return true;
	}

	cxl_endpoint_foreach(port, endpoint)
		if (__memdev_filter_by_decoder(
			    memdev, cxl_endpoint_get_port(endpoint), ident))
			return true;
	return false;
}

static struct cxl_memdev *
util_cxl_memdev_filter_by_decoder(struct cxl_memdev *memdev, const char *ident)
{
	struct cxl_ctx *ctx = cxl_memdev_get_ctx(memdev);
	struct cxl_bus *bus;

	if (!ident)
		return memdev;

	cxl_bus_foreach(ctx, bus) {
		struct cxl_port *port, *top;

		port = cxl_bus_get_port(bus);
		if (__memdev_filter_by_decoder(memdev, port, ident))
			return memdev;

		top = port;
		cxl_port_foreach_all(top, port)
			if (__memdev_filter_by_decoder(memdev, port, ident))
				return memdev;
	}

	return NULL;
}

static bool __memdev_filter_by_port(struct cxl_memdev *memdev,
				    struct cxl_port *port,
				    const char *port_ident)
{
	struct cxl_endpoint *endpoint;

	if (util_cxl_port_filter(port, port_ident, CXL_PF_SINGLE) &&
	    cxl_port_get_dport_by_memdev(port, memdev))
		return true;

	cxl_endpoint_foreach(port, endpoint)
		if (__memdev_filter_by_port(memdev,
					    cxl_endpoint_get_port(endpoint),
					    port_ident))
			return true;
	return false;
}

static struct cxl_memdev *
util_cxl_memdev_filter_by_port(struct cxl_memdev *memdev, const char *bus_ident,
			       const char *port_ident)
{
	struct cxl_ctx *ctx = cxl_memdev_get_ctx(memdev);
	struct cxl_bus *bus;

	if (!bus_ident && !port_ident)
		return memdev;

	cxl_bus_foreach(ctx, bus) {
		struct cxl_port *port, *top;

		port = cxl_bus_get_port(bus);
		if (util_cxl_bus_filter(bus, bus_ident))
			if (__memdev_filter_by_port(memdev, port,
						    cxl_bus_get_devname(bus)))
				return memdev;
		if (__memdev_filter_by_port(memdev, port, port_ident))
				return memdev;
		top = port;
		cxl_port_foreach_all(top, port)
			if (__memdev_filter_by_port(memdev, port, port_ident))
				return memdev;
	}

	return NULL;
}

static unsigned long params_to_flags(struct cxl_filter_params *param)
{
	unsigned long flags = 0;

	if (param->idle)
		flags |= UTIL_JSON_IDLE;
	if (param->human)
		flags |= UTIL_JSON_HUMAN;
	if (param->health)
		flags |= UTIL_JSON_HEALTH;
	if (param->targets)
		flags |= UTIL_JSON_TARGETS;
	return flags;
}

static void splice_array(struct cxl_filter_params *p, struct json_object *jobjs,
			 struct json_object *platform,
			 const char *container_name, bool do_container)
{
	size_t count;

	if (!json_object_array_length(jobjs)) {
		json_object_put(jobjs);
		return;
	}

	if (do_container) {
		struct json_object *container = json_object_new_object();

		if (!container) {
			err(p, "failed to list: %s\n", container_name);
			return;
		}

		json_object_object_add(container, container_name, jobjs);
		json_object_array_add(platform, container);
		return;
	}

	for (count = json_object_array_length(jobjs); count; count--) {
		struct json_object *jobj = json_object_array_get_idx(jobjs, 0);

		json_object_get(jobj);
		json_object_array_del_idx(jobjs, 0, 1);
		json_object_array_add(platform, jobj);
	}
	json_object_put(jobjs);
}

static bool cond_add_put_array(struct json_object *jobj, const char *key,
			       struct json_object *array)
{
	if (jobj && array && json_object_array_length(array) > 0) {
		json_object_object_add(jobj, key, array);
		return true;
	} else {
		json_object_put(array);
		return false;
	}
}

static bool cond_add_put_array_suffix(struct json_object *jobj, const char *key,
				      const char *suffix,
				      struct json_object *array)
{
	char *name;
	bool rc;

	if (asprintf(&name, "%s:%s", key, suffix) < 0)
		return false;
	rc = cond_add_put_array(jobj, name, array);
	free(name);
	return rc;
}

static struct json_object *pick_array(struct json_object *child,
				      struct json_object *container)
{
	if (child)
		return child;
	if (container)
		return container;
	return NULL;
}

static void walk_decoders(struct cxl_port *port, struct cxl_filter_params *p,
			  struct json_object *jdecoders, unsigned long flags)
{
	struct cxl_decoder *decoder;

	cxl_decoder_foreach(port, decoder) {
		struct json_object *jdecoder;

		if (!p->decoders)
			continue;
		if (!util_cxl_decoder_filter(decoder, p->decoder_filter))
			continue;
		if (!util_cxl_decoder_filter_by_bus(decoder, p->bus_filter))
			continue;
		if (!util_cxl_decoder_filter_by_port(decoder, p->port_filter,
						     pf_mode(p)))
			continue;
		if (!util_cxl_decoder_filter_by_memdev(
			    decoder, p->memdev_filter, p->serial_filter))
			continue;
		if (!p->idle && cxl_decoder_get_size(decoder) == 0)
			continue;
		jdecoder = util_cxl_decoder_to_json(decoder, flags);
		if (!decoder) {
			dbg(p, "decoder object allocation failure\n");
			continue;
		}
		json_object_array_add(jdecoders, jdecoder);
	}
}

static void walk_endpoints(struct cxl_port *port, struct cxl_filter_params *p,
			   struct json_object *jeps, struct json_object *jdevs,
			   struct json_object *jdecoders, unsigned long flags)
{
	struct cxl_endpoint *endpoint;

	cxl_endpoint_foreach(port, endpoint) {
		struct cxl_port *ep_port = cxl_endpoint_get_port(endpoint);
		const char *devname = cxl_endpoint_get_devname(endpoint);
		struct json_object *jchilddecoders = NULL;
		struct json_object *jendpoint = NULL;
		struct cxl_memdev *memdev;

		if (!util_cxl_endpoint_filter(endpoint, p->endpoint_filter))
			continue;
		if (!util_cxl_port_filter_by_bus(ep_port, p->bus_filter))
			continue;
		if (!util_cxl_endpoint_filter_by_port(endpoint, p->port_filter,
						      pf_mode(p)))
			continue;
		if (!util_cxl_endpoint_filter_by_memdev(
			    endpoint, p->memdev_filter, p->serial_filter))
			continue;
		if (!p->idle && !cxl_endpoint_is_enabled(endpoint))
			continue;
		if (p->endpoints) {
			jendpoint = util_cxl_endpoint_to_json(endpoint, flags);
			if (!jendpoint) {
				err(p, "%s: failed to list\n", devname);
				continue;
			}
			json_object_array_add(jeps, jendpoint);
		}
		if (p->memdevs) {
			struct json_object *jobj;

			memdev = cxl_endpoint_get_memdev(endpoint);
			if (!memdev)
				continue;
			if (!util_cxl_memdev_filter(memdev, p->memdev_filter,
						    p->serial_filter))
				continue;
			if (!util_cxl_memdev_filter_by_decoder(
				    memdev, p->decoder_filter))
				continue;
			if (!util_cxl_memdev_filter_by_port(
				    memdev, p->bus_filter, p->port_filter))
				continue;
			if (!p->idle && !cxl_memdev_is_enabled(memdev))
				continue;
			jobj = util_cxl_memdev_to_json(memdev, flags);
			if (!jobj) {
				err(p, "failed to json serialize %s\n",
				    cxl_memdev_get_devname(memdev));
				continue;
			}
			if (p->endpoints)
				json_object_object_add(jendpoint, "memdev",
						       jobj);
			else
				json_object_array_add(jdevs, jobj);
		}

		if (p->decoders && p->endpoints) {
			jchilddecoders = json_object_new_array();
			if (!jchilddecoders) {
				err(p,
				    "%s: failed to enumerate child decoders\n",
				    devname);
				continue;
			}
		}

		if (!p->decoders)
			continue;
		walk_decoders(cxl_endpoint_get_port(endpoint), p,
			      pick_array(jchilddecoders, jdecoders), flags);
		cond_add_put_array_suffix(jendpoint, "decoders", devname,
					  jchilddecoders);
	}
}

static void
walk_child_ports(struct cxl_port *parent_port, struct cxl_filter_params *p,
		 struct json_object *jports, struct json_object *jportdecoders,
		 struct json_object *jeps, struct json_object *jepdecoders,
		 struct json_object *jdevs, unsigned long flags)
{
	struct cxl_port *port;

	cxl_port_foreach(parent_port, port) {
		const char *devname = cxl_port_get_devname(port);
		struct json_object *jport = NULL;
		struct json_object *jchilddevs = NULL;
		struct json_object *jchildports = NULL;
		struct json_object *jchildeps = NULL;
		struct json_object *jchilddecoders = NULL;

		if (!util_cxl_port_filter_by_memdev(port, p->memdev_filter,
						    p->serial_filter))
			continue;
		if (!util_cxl_port_filter(port, p->port_filter, pf_mode(p)))
			goto walk_children;
		if (!util_cxl_port_filter_by_bus(port, p->bus_filter))
			goto walk_children;
		if (!p->idle && !cxl_port_is_enabled(port))
			continue;
		if (p->ports) {
			jport = util_cxl_port_to_json(port, flags);
			if (!jport) {
				err(p, "%s: failed to list\n", devname);
				continue;
			}
			json_object_array_add(jports, jport);
			jchildports = json_object_new_array();
			if (!jchildports) {
				err(p, "%s: failed to enumerate child ports\n",
				    devname);
				continue;
			}

			if (p->memdevs && !p->endpoints) {
				jchilddevs = json_object_new_array();
				if (!jchilddevs) {
					err(p,
					    "%s: failed to enumerate child memdevs\n",
					    devname);
					continue;
				}
			}

			if (p->endpoints) {
				jchildeps = json_object_new_array();
				if (!jchildeps) {
					err(p,
					    "%s: failed to enumerate child endpoints\n",
					    devname);
					continue;
				}
			}

			if (p->decoders) {
				jchilddecoders = json_object_new_array();
				if (!jchilddecoders) {
					err(p,
					    "%s: failed to enumerate child decoders\n",
					    devname);
					continue;
				}
			}
		}

walk_children:
		if (p->endpoints || p->memdevs || p->decoders)
			walk_endpoints(port, p, pick_array(jchildeps, jeps),
				       pick_array(jchilddevs, jdevs),
				       pick_array(jchilddecoders, jepdecoders),
				       flags);

		walk_decoders(port, p,
			      pick_array(jchilddecoders, jportdecoders), flags);
		walk_child_ports(port, p, pick_array(jchildports, jports),
				 pick_array(jchilddecoders, jportdecoders),
				 pick_array(jchildeps, jeps),
				 pick_array(jchilddecoders, jepdecoders),
				 pick_array(jchilddevs, jdevs), flags);
		cond_add_put_array_suffix(jport, "ports", devname, jchildports);
		cond_add_put_array_suffix(jport, "endpoints", devname,
					  jchildeps);
		cond_add_put_array_suffix(jport, "decoders", devname,
					  jchilddecoders);
		cond_add_put_array_suffix(jport, "memdevs", devname,
					  jchilddevs);
	}
}

int cxl_filter_walk(struct cxl_ctx *ctx, struct cxl_filter_params *p)
{
	struct json_object *jdevs = NULL, *jbuses = NULL, *jports = NULL;
	struct json_object *jplatform = json_object_new_array();
	unsigned long flags = params_to_flags(p);
	struct json_object *jportdecoders = NULL;
	struct json_object *jbusdecoders = NULL;
	struct json_object *jepdecoders = NULL;
	struct json_object *janondevs = NULL;
	struct json_object *jeps = NULL;
	struct cxl_memdev *memdev;
	int top_level_objs = 0;
	struct cxl_bus *bus;

	if (!jplatform) {
		dbg(p, "platform object allocation failure\n");
		return -ENOMEM;
	}

	janondevs = json_object_new_array();
	if (!janondevs)
		goto err;

	jbuses = json_object_new_array();
	if (!jbuses)
		goto err;

	jports = json_object_new_array();
	if (!jports)
		goto err;

	jeps = json_object_new_array();
	if (!jeps)
		goto err;

	jdevs = json_object_new_array();
	if (!jdevs)
		goto err;

	jbusdecoders = json_object_new_array();
	if (!jbusdecoders)
		goto err;

	jportdecoders = json_object_new_array();
	if (!jportdecoders)
		goto err;

	jepdecoders = json_object_new_array();
	if (!jepdecoders)
		goto err;

	dbg(p, "walk memdevs\n");
	cxl_memdev_foreach(ctx, memdev) {
		struct json_object *janondev;

		if (!util_cxl_memdev_filter(memdev, p->memdev_filter,
					    p->serial_filter))
			continue;
		if (cxl_memdev_is_enabled(memdev))
			continue;
		if (!p->idle)
			continue;
		if (p->memdevs) {
			janondev = util_cxl_memdev_to_json(memdev, flags);
			if (!janondev) {
				dbg(p, "memdev object allocation failure\n");
				continue;
			}
			json_object_array_add(janondevs, janondev);
		}
	}

	dbg(p, "walk buses\n");
	cxl_bus_foreach(ctx, bus) {
		struct json_object *jbus = NULL;
		struct json_object *jchilddecoders = NULL;
		struct json_object *jchildports = NULL;
		struct json_object *jchilddevs = NULL;
		struct json_object *jchildeps = NULL;
		struct cxl_port *port = cxl_bus_get_port(bus);
		const char *devname = cxl_bus_get_devname(bus);

		if (!util_cxl_bus_filter_by_memdev(bus, p->memdev_filter,
						   p->serial_filter))
			continue;
		if (!util_cxl_bus_filter(bus, p->bus_filter))
			goto walk_children;
		if (!util_cxl_port_filter(port, p->port_filter, pf_mode(p)))
			goto walk_children;
		if (p->buses) {
			jbus = util_cxl_bus_to_json(bus, flags);
			if (!jbus) {
				dbg(p, "bus object allocation failure\n");
				continue;
			}
			json_object_array_add(jbuses, jbus);
			if (p->ports) {
				jchildports = json_object_new_array();
				if (!jchildports) {
					err(p,
					    "%s: failed to enumerate child ports\n",
					    devname);
					continue;
				}
			}
			if (p->endpoints) {
				jchildeps = json_object_new_array();
				if (!jchildeps) {
					err(p,
					    "%s: failed to enumerate child endpoints\n",
					    devname);
					continue;
				}
			}

			if (p->memdevs && !p->ports && !p->endpoints) {
				jchilddevs = json_object_new_array();
				if (!jchilddevs) {
					err(p,
					    "%s: failed to enumerate child memdevs\n",
					    devname);
					continue;
				}
			}
			if (p->decoders) {
				jchilddecoders = json_object_new_array();
				if (!jchilddecoders) {
					err(p,
					    "%s: failed to enumerate child decoders\n",
					    devname);
					continue;
				}
			}

		}
walk_children:
		dbg(p, "walk decoders\n");
		walk_decoders(port, p, pick_array(jchilddecoders, jbusdecoders),
			      flags);

		dbg(p, "walk ports\n");
		walk_child_ports(port, p, pick_array(jchildports, jports),
				 pick_array(jchilddecoders, jportdecoders),
				 pick_array(jchildeps, jeps),
				 pick_array(jchilddecoders, jepdecoders),
				 pick_array(jchilddevs, jdevs), flags);
		cond_add_put_array_suffix(jbus, "ports", devname, jchildports);
		cond_add_put_array_suffix(jbus, "endpoints", devname,
					  jchildeps);
		cond_add_put_array_suffix(jbus, "decoders", devname,
					  jchilddecoders);
		cond_add_put_array_suffix(jbus, "memdevs", devname, jchilddevs);
	}

	if (json_object_array_length(janondevs))
		top_level_objs++;
	if (json_object_array_length(jbuses))
		top_level_objs++;
	if (json_object_array_length(jports))
		top_level_objs++;
	if (json_object_array_length(jeps))
		top_level_objs++;
	if (json_object_array_length(jdevs))
		top_level_objs++;
	if (json_object_array_length(jbusdecoders))
		top_level_objs++;
	if (json_object_array_length(jportdecoders))
		top_level_objs++;
	if (json_object_array_length(jepdecoders))
		top_level_objs++;

	splice_array(p, janondevs, jplatform, "anon memdevs", top_level_objs > 1);
	splice_array(p, jbuses, jplatform, "buses", top_level_objs > 1);
	splice_array(p, jports, jplatform, "ports", top_level_objs > 1);
	splice_array(p, jeps, jplatform, "endpoints", top_level_objs > 1);
	splice_array(p, jdevs, jplatform, "memdevs", top_level_objs > 1);
	splice_array(p, jbusdecoders, jplatform, "root decoders",
		     top_level_objs > 1);
	splice_array(p, jportdecoders, jplatform, "port decoders",
		     top_level_objs > 1);
	splice_array(p, jepdecoders, jplatform, "endpoint decoders",
		     top_level_objs > 1);

	util_display_json_array(stdout, jplatform, flags);

	return 0;
err:
	json_object_put(janondevs);
	json_object_put(jbuses);
	json_object_put(jports);
	json_object_put(jeps);
	json_object_put(jdevs);
	json_object_put(jbusdecoders);
	json_object_put(jportdecoders);
	json_object_put(jepdecoders);
	json_object_put(jplatform);
	return -ENOMEM;
}
