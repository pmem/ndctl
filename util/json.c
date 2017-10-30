/*
 * Copyright(c) 2015-2017 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */
#include <limits.h>
#include <string.h>
#include <util/json.h>
#include <util/filter.h>
#include <uuid/uuid.h>
#include <json-c/json.h>
#include <json-c/printbuf.h>
#include <ndctl/libndctl.h>
#include <daxctl/libdaxctl.h>
#include <ccan/array_size/array_size.h>
#include <ccan/short_types/short_types.h>

#ifdef HAVE_NDCTL_H
#include <linux/ndctl.h>
#else
#include <ndctl.h>
#endif

/* adapted from mdadm::human_size_brief() */
static int display_size(struct json_object *jobj, struct printbuf *pbuf,
		int level, int flags)
{
	unsigned long long bytes = json_object_get_int64(jobj);
	static char buf[128];
	int c;

	/*
	 * We convert bytes to either centi-M{ega,ibi}bytes or
	 * centi-G{igi,ibi}bytes, with appropriate rounding, and then print
	 * 1/100th of those as a decimal.  We allow upto 2048Megabytes before
	 * converting to gigabytes, as that shows more precision and isn't too
	 * large a number.  Terabytes are not yet handled.
	 *
	 * If prefix == IEC, we mean prefixes like kibi,mebi,gibi etc.
	 * If prefix == JEDEC, we mean prefixes like kilo,mega,giga etc.
	 */

	if (bytes < 5000*1024)
		snprintf(buf, sizeof(buf), "%lld", bytes);
	else {
		/* IEC */
		if (bytes < 2*1024LL*1024LL*1024LL) {
			long cMiB = (bytes * 200LL / (1LL<<20) +1) /2;

			c = snprintf(buf, sizeof(buf), "\"%ld.%02ld MiB",
					cMiB/100 , cMiB % 100);
		} else {
			long cGiB = (bytes * 200LL / (1LL<<30) +1) /2;

			c = snprintf(buf, sizeof(buf), "\"%ld.%02ld GiB",
					cGiB/100 , cGiB % 100);
		}

		/* JEDEC */
		if (bytes < 2*1024LL*1024LL*1024LL) {
			long cMB  = (bytes / (1000000LL / 200LL) + 1) / 2;

			snprintf(buf + c, sizeof(buf) - c, " (%ld.%02ld MB)\"",
					cMB/100, cMB % 100);
		} else {
			long cGB  = (bytes / (1000000000LL/200LL) + 1) / 2;

			snprintf(buf + c, sizeof(buf) - c, " (%ld.%02ld GB)\"",
					cGB/100 , cGB % 100);
		}
	}

	return printbuf_memappend(pbuf, buf, strlen(buf));
}

static int display_hex(struct json_object *jobj, struct printbuf *pbuf,
		int level, int flags)
{
	unsigned long long val = json_object_get_int64(jobj);
	static char buf[32];

	snprintf(buf, sizeof(buf), "\"%#llx\"", val);
	return printbuf_memappend(pbuf, buf, strlen(buf));
}

struct json_object *util_json_object_size(unsigned long long size,
		unsigned long flags)
{
	struct json_object *jobj = json_object_new_int64(size);

	if (jobj && (flags & UTIL_JSON_HUMAN))
		json_object_set_serializer(jobj, display_size, NULL, NULL);
	return jobj;
}

struct json_object *util_json_object_hex(unsigned long long val,
		unsigned long flags)
{
	struct json_object *jobj = json_object_new_int64(val);

	if (jobj && (flags & UTIL_JSON_HUMAN))
		json_object_set_serializer(jobj, display_hex, NULL, NULL);
	return jobj;
}

void util_display_json_array(FILE *f_out, struct json_object *jarray, int jflag)
{
	int len = json_object_array_length(jarray);

	if (json_object_array_length(jarray) > 1)
		fprintf(f_out, "%s\n", json_object_to_json_string_ext(jarray, jflag));
	else if (len) {
		struct json_object *jobj;

		jobj = json_object_array_get_idx(jarray, 0);
		fprintf(f_out, "%s\n", json_object_to_json_string_ext(jobj, jflag));
	}
	json_object_put(jarray);
}

struct json_object *util_bus_to_json(struct ndctl_bus *bus)
{
	struct json_object *jbus = json_object_new_object();
	struct json_object *jobj;

	if (!jbus)
		return NULL;

	jobj = json_object_new_string(ndctl_bus_get_provider(bus));
	if (!jobj)
		goto err;
	json_object_object_add(jbus, "provider", jobj);

	jobj = json_object_new_string(ndctl_bus_get_devname(bus));
	if (!jobj)
		goto err;
	json_object_object_add(jbus, "dev", jobj);

	return jbus;
 err:
	json_object_put(jbus);
	return NULL;
}

struct json_object *util_dimm_to_json(struct ndctl_dimm *dimm,
		unsigned long flags)
{
	struct json_object *jdimm = json_object_new_object();
	const char *id = ndctl_dimm_get_unique_id(dimm);
	unsigned int handle = ndctl_dimm_get_handle(dimm);
	unsigned short phys_id = ndctl_dimm_get_phys_id(dimm);
	struct json_object *jobj;

	if (!jdimm)
		return NULL;

	jobj = json_object_new_string(ndctl_dimm_get_devname(dimm));
	if (!jobj)
		goto err;
	json_object_object_add(jdimm, "dev", jobj);

	if (id) {
		jobj = json_object_new_string(id);
		if (!jobj)
			goto err;
		json_object_object_add(jdimm, "id", jobj);
	}

	if (handle < UINT_MAX) {
		jobj = util_json_object_hex(handle, flags);
		if (!jobj)
			goto err;
		json_object_object_add(jdimm, "handle", jobj);
	}

	if (phys_id < USHRT_MAX) {
		jobj = util_json_object_hex(phys_id, flags);
		if (!jobj)
			goto err;
		json_object_object_add(jdimm, "phys_id", jobj);
	}

	if (!ndctl_dimm_is_enabled(dimm)) {
		jobj = json_object_new_string("disabled");
		if (!jobj)
			goto err;
		json_object_object_add(jdimm, "state", jobj);
	}

	if (ndctl_dimm_failed_map(dimm)) {
		jobj = json_object_new_boolean(true);
		if (!jobj)
			goto err;
		json_object_object_add(jdimm, "flag_failed_map", jobj);
	}

	if (ndctl_dimm_failed_save(dimm)) {
		jobj = json_object_new_boolean(true);
		if (!jobj)
			goto err;
		json_object_object_add(jdimm, "flag_failed_save", jobj);
	}

	if (ndctl_dimm_failed_arm(dimm)) {
		jobj = json_object_new_boolean(true);
		if (!jobj)
			goto err;
		json_object_object_add(jdimm, "flag_failed_arm", jobj);
	}

	if (ndctl_dimm_failed_restore(dimm)) {
		jobj = json_object_new_boolean(true);
		if (!jobj)
			goto err;
		json_object_object_add(jdimm, "flag_failed_restore", jobj);
	}

	if (ndctl_dimm_failed_flush(dimm)) {
		jobj = json_object_new_boolean(true);
		if (!jobj)
			goto err;
		json_object_object_add(jdimm, "flag_failed_flush", jobj);
	}

	if (ndctl_dimm_smart_pending(dimm)) {
		jobj = json_object_new_boolean(true);
		if (!jobj)
			goto err;
		json_object_object_add(jdimm, "flag_smart_event", jobj);
	}

	return jdimm;
 err:
	json_object_put(jdimm);
	return NULL;
}

struct json_object *util_daxctl_dev_to_json(struct daxctl_dev *dev,
		unsigned long flags)
{
	const char *devname = daxctl_dev_get_devname(dev);
	struct json_object *jdev, *jobj;

	jdev = json_object_new_object();
	if (!devname || !jdev)
		return NULL;

	jobj = json_object_new_string(devname);
	if (jobj)
		json_object_object_add(jdev, "chardev", jobj);

	jobj = util_json_object_size(daxctl_dev_get_size(dev), flags);
	if (jobj)
		json_object_object_add(jdev, "size", jobj);

	return jdev;
}

struct json_object *util_daxctl_devs_to_list(struct daxctl_region *region,
		struct json_object *jdevs, const char *ident,
		unsigned long flags)
{
	struct daxctl_dev *dev;

	daxctl_dev_foreach(region, dev) {
		struct json_object *jdev;

		if (!util_daxctl_dev_filter(dev, ident))
			continue;

		if (!(flags & UTIL_JSON_IDLE) && !daxctl_dev_get_size(dev))
			continue;

		if (!jdevs) {
			jdevs = json_object_new_array();
			if (!jdevs)
				return NULL;
		}

		jdev = util_daxctl_dev_to_json(dev, flags);
		if (!jdev) {
			json_object_put(jdevs);
			return NULL;
		}

		json_object_array_add(jdevs, jdev);
	}

	return jdevs;
}

struct json_object *util_daxctl_region_to_json(struct daxctl_region *region,
		const char *ident, unsigned long flags)
{
	unsigned long align;
	struct json_object *jregion, *jobj;
	unsigned long long available_size, size;

	jregion = json_object_new_object();
	if (!jregion)
		return NULL;

	/*
	 * The flag indicates when we are being called by an agent that
	 * already knows about the parent device information.
	 */
	if (!(flags & UTIL_JSON_DAX)) {
		/* trim off the redundant /sys/devices prefix */
		const char *path = daxctl_region_get_path(region);
		int len = strlen("/sys/devices");
		const char *trim = &path[len];

		if (strncmp(path, "/sys/devices", len) != 0)
			goto err;
		jobj = json_object_new_string(trim);
		if (!jobj)
			goto err;
		json_object_object_add(jregion, "path", jobj);
	}

	jobj = json_object_new_int(daxctl_region_get_id(region));
	if (!jobj)
		goto err;
	json_object_object_add(jregion, "id", jobj);

	size = daxctl_region_get_size(region);
	if (size < ULLONG_MAX) {
		jobj = util_json_object_size(size, flags);
		if (!jobj)
			goto err;
		json_object_object_add(jregion, "size", jobj);
	}

	available_size = daxctl_region_get_available_size(region);
	if (available_size) {
		jobj = util_json_object_size(available_size, flags);
		if (!jobj)
			goto err;
		json_object_object_add(jregion, "available_size", jobj);
	}

	align = daxctl_region_get_align(region);
	if (align < ULONG_MAX) {
		jobj = json_object_new_int64(align);
		if (!jobj)
			goto err;
		json_object_object_add(jregion, "align", jobj);
	}

	if (!(flags & UTIL_JSON_DAX_DEVS))
		return jregion;

	jobj = util_daxctl_devs_to_list(region, NULL, ident, flags);
	if (jobj)
		json_object_object_add(jregion, "devices", jobj);

	return jregion;
 err:
	json_object_put(jregion);
	return NULL;
}

static int compare_dimm_number(const void *p1, const void *p2)
{
	struct ndctl_dimm *dimm1 = *(struct ndctl_dimm **)p1;
	struct ndctl_dimm *dimm2 = *(struct ndctl_dimm **)p2;
	const char *dimm1_name = ndctl_dimm_get_devname(dimm1);
	const char *dimm2_name = ndctl_dimm_get_devname(dimm2);
	int num1, num2;

	if (sscanf(dimm1_name, "nmem%d", &num1) != 1)
		num1 = 0;
	if (sscanf(dimm2_name, "nmem%d", &num2) != 1)
		num2 = 0;

	return num1 - num2;
}

static struct json_object *badblocks_to_jdimms(struct ndctl_region *region,
		unsigned long long addr, unsigned long len)
{
	struct ndctl_bus *bus = ndctl_region_get_bus(region);
	int count = ndctl_region_get_interleave_ways(region);
	unsigned long long end = addr + len;
	struct json_object *jdimms, *jobj;
	struct ndctl_dimm **dimms, *dimm;
	int found, i;

	jdimms = json_object_new_array();
	if (!jdimms)
		return NULL;

	dimms = calloc(count, sizeof(struct ndctl_dimm *));
	if (!dimms)
		goto err_dimms;

	for (found = 0; found < count && addr < end; addr += 512) {
		dimm = ndctl_bus_get_dimm_by_physical_address(bus, addr);
		if (!dimm)
			continue;

		for (i = 0; i < count; i++)
			if (dimms[i] == dimm)
				break;
		if (i >= count)
			dimms[found++] = dimm;
	}

	if (!found)
		goto err_found;

	qsort(dimms, found, sizeof(dimm), compare_dimm_number);

	for (i = 0; i < found; i++) {
		const char *devname = ndctl_dimm_get_devname(dimms[i]);

		jobj = json_object_new_string(devname);
		if (!jobj)
			break;
		json_object_array_add(jdimms, jobj);
	}

	if (!i)
		goto err_found;
	free(dimms);
	return jdimms;

err_found:
	free(dimms);
err_dimms:
	json_object_put(jdimms);
	return NULL;
}

struct json_object *util_region_badblocks_to_json(struct ndctl_region *region,
		unsigned int *bb_count, unsigned long flags)
{
	struct json_object *jbb = NULL, *jbbs = NULL, *jobj;
	struct badblock *bb;
	int bbs = 0;

	if (flags & UTIL_JSON_MEDIA_ERRORS) {
		jbbs = json_object_new_array();
		if (!jbbs)
			return NULL;
	}

	ndctl_region_badblock_foreach(region, bb) {
		struct json_object *jdimms;
		unsigned long long addr;

		bbs += bb->len;

		if (!(flags & UTIL_JSON_MEDIA_ERRORS))
			continue;

		/* get start address of region */
		addr = ndctl_region_get_resource(region);
		if (addr == ULONG_MAX)
			goto err_array;

		/* get address of bad block */
		addr += bb->offset << 9;

		jbb = json_object_new_object();
		if (!jbb)
			goto err_array;

		jobj = json_object_new_int64(bb->offset);
		if (!jobj)
			goto err;
		json_object_object_add(jbb, "offset", jobj);

		jobj = json_object_new_int(bb->len);
		if (!jobj)
			goto err;
		json_object_object_add(jbb, "length", jobj);

		jdimms = badblocks_to_jdimms(region, addr, bb->len << 9);
		if (jdimms)
			json_object_object_add(jbb, "dimms", jdimms);
		json_object_array_add(jbbs, jbb);
	}

	*bb_count = bbs;

	if (bbs)
		return jbbs;

 err:
	json_object_put(jbb);
 err_array:
	json_object_put(jbbs);
	return NULL;
}

static struct json_object *dev_badblocks_to_json(struct ndctl_region *region,
		unsigned long long dev_begin, unsigned long long dev_size,
		unsigned int *bb_count, unsigned long flags)
{
	struct json_object *jbb = NULL, *jbbs = NULL, *jobj;
	unsigned long long region_begin, dev_end, offset;
	unsigned int len, bbs = 0;
	struct badblock *bb;

	region_begin = ndctl_region_get_resource(region);
	if (region_begin == ULLONG_MAX)
		return NULL;

	dev_end = dev_begin + dev_size - 1;

	if (flags & UTIL_JSON_MEDIA_ERRORS) {
		jbbs = json_object_new_array();
		if (!jbbs)
			return NULL;
	}

	ndctl_region_badblock_foreach(region, bb) {
		unsigned long long bb_begin, bb_end, begin, end;
		struct json_object *jdimms;

		bb_begin = region_begin + (bb->offset << 9);
		bb_end = bb_begin + (bb->len << 9) - 1;

		if (bb_end <= dev_begin || bb_begin >= dev_end)
			continue;

		if (bb_begin < dev_begin)
			begin = dev_begin;
		else
			begin = bb_begin;

		if (bb_end > dev_end)
			end = dev_end;
		else
			end = bb_end;

		offset = (begin - dev_begin) >> 9;
		len = (end - begin + 1) >> 9;

		bbs += len;

		if (!(flags & UTIL_JSON_MEDIA_ERRORS))
			continue;

		jbb = json_object_new_object();
		if (!jbb)
			goto err_array;

		jobj = json_object_new_int64(offset);
		if (!jobj)
			goto err;
		json_object_object_add(jbb, "offset", jobj);

		jobj = json_object_new_int(len);
		if (!jobj)
			goto err;
		json_object_object_add(jbb, "length", jobj);

		jdimms = badblocks_to_jdimms(region, begin, len << 9);
		if (jdimms)
			json_object_object_add(jbb, "dimms", jdimms);

		json_object_array_add(jbbs, jbb);
	}

	*bb_count = bbs;

	if (bbs)
		return jbbs;

 err:
	json_object_put(jbb);
 err_array:
	json_object_put(jbbs);
	return NULL;
}

static struct json_object *util_pfn_badblocks_to_json(struct ndctl_pfn *pfn,
		unsigned int *bb_count, unsigned long flags)
{
	struct ndctl_region *region = ndctl_pfn_get_region(pfn);
	unsigned long long pfn_begin, pfn_size;

	pfn_begin = ndctl_pfn_get_resource(pfn);
	if (pfn_begin == ULLONG_MAX)
		return NULL;

	pfn_size = ndctl_pfn_get_size(pfn);
	if (pfn_size == ULLONG_MAX)
		return NULL;

	return dev_badblocks_to_json(region, pfn_begin, pfn_size,
			bb_count, flags);
}

static void util_btt_badblocks_to_json(struct ndctl_btt *btt,
		unsigned int *bb_count)
{
	struct ndctl_region *region = ndctl_btt_get_region(btt);
	struct ndctl_namespace *ndns = ndctl_btt_get_namespace(btt);
	unsigned long long begin, size;

	begin = ndctl_namespace_get_resource(ndns);
	if (begin == ULLONG_MAX)
		return;

	size = ndctl_namespace_get_size(ndns);
	if (size == ULLONG_MAX)
		return;

	/*
	 * The dev_badblocks_to_json() for BTT is not accurate with
	 * respect to data vs metadata badblocks, and is only useful for
	 * a potential bb_count.
	 *
	 * FIXME: switch to native BTT badblocks representation
	 * when / if the kernel provides it.
	 */
	dev_badblocks_to_json(region, begin, size, bb_count, 0);
}

static struct json_object *util_dax_badblocks_to_json(struct ndctl_dax *dax,
		unsigned int *bb_count, unsigned long flags)
{
	struct ndctl_region *region = ndctl_dax_get_region(dax);
	unsigned long long dax_begin, dax_size;

	dax_begin = ndctl_dax_get_resource(dax);
	if (dax_begin == ULLONG_MAX)
		return NULL;

	dax_size = ndctl_dax_get_size(dax);
	if (dax_size == ULLONG_MAX)
		return NULL;

	return dev_badblocks_to_json(region, dax_begin, dax_size,
			bb_count, flags);
}

struct json_object *util_namespace_to_json(struct ndctl_namespace *ndns,
		unsigned long flags)
{
	struct json_object *jndns = json_object_new_object();
	struct json_object *jobj, *jbbs = NULL;
	unsigned long long size = ULLONG_MAX;
	enum ndctl_namespace_mode mode;
	const char *bdev = NULL, *name;
	unsigned int bb_count = 0;
	struct ndctl_btt *btt;
	struct ndctl_pfn *pfn;
	struct ndctl_dax *dax;
	char buf[40];
	uuid_t uuid;
	int numa;

	if (!jndns)
		return NULL;

	jobj = json_object_new_string(ndctl_namespace_get_devname(ndns));
	if (!jobj)
		goto err;
	json_object_object_add(jndns, "dev", jobj);

	btt = ndctl_namespace_get_btt(ndns);
	dax = ndctl_namespace_get_dax(ndns);
	pfn = ndctl_namespace_get_pfn(ndns);
	mode = ndctl_namespace_get_mode(ndns);
	switch (mode) {
	case NDCTL_NS_MODE_MEMORY:
		if (pfn) /* dynamic memory mode */
			size = ndctl_pfn_get_size(pfn);
		else /* native/static memory mode */
			size = ndctl_namespace_get_size(ndns);
		jobj = json_object_new_string("memory");
		break;
	case NDCTL_NS_MODE_DAX:
		if (!dax)
			goto err;
		size = ndctl_dax_get_size(dax);
		jobj = json_object_new_string("dax");
		break;
	case NDCTL_NS_MODE_SAFE:
		if (!btt)
			goto err;
		jobj = json_object_new_string("sector");
		size = ndctl_btt_get_size(btt);
		break;
	case NDCTL_NS_MODE_RAW:
		size = ndctl_namespace_get_size(ndns);
		jobj = json_object_new_string("raw");
		break;
	default:
		jobj = NULL;
	}
	if (jobj)
		json_object_object_add(jndns, "mode", jobj);

	if (size < ULLONG_MAX) {
		jobj = util_json_object_size(size, flags);
		if (jobj)
			json_object_object_add(jndns, "size", jobj);
	}

	if (btt) {
		ndctl_btt_get_uuid(btt, uuid);
		uuid_unparse(uuid, buf);
		jobj = json_object_new_string(buf);
		if (!jobj)
			goto err;
		json_object_object_add(jndns, "uuid", jobj);

		jobj = json_object_new_int(ndctl_btt_get_sector_size(btt));
		if (!jobj)
			goto err;
		json_object_object_add(jndns, "sector_size", jobj);

		bdev = ndctl_btt_get_block_device(btt);
	} else if (pfn) {
		ndctl_pfn_get_uuid(pfn, uuid);
		uuid_unparse(uuid, buf);
		jobj = json_object_new_string(buf);
		if (!jobj)
			goto err;
		json_object_object_add(jndns, "uuid", jobj);
		bdev = ndctl_pfn_get_block_device(pfn);
	} else if (dax) {
		struct daxctl_region *dax_region;

		dax_region = ndctl_dax_get_daxctl_region(dax);
		ndctl_dax_get_uuid(dax, uuid);
		uuid_unparse(uuid, buf);
		jobj = json_object_new_string(buf);
		if (!jobj)
			goto err;
		json_object_object_add(jndns, "uuid", jobj);
		if ((flags & UTIL_JSON_DAX) && dax_region) {
			jobj = util_daxctl_region_to_json(dax_region, NULL,
					flags);
			if (jobj)
				json_object_object_add(jndns, "daxregion", jobj);
		} else if (dax_region) {
			struct daxctl_dev *dev;

			/*
			 * We can only find/list these device-dax
			 * details when the instance is enabled.
			 */
			dev = daxctl_dev_get_first(dax_region);
			if (dev) {
				name = daxctl_dev_get_devname(dev);
				jobj = json_object_new_string(name);
				if (!jobj)
					goto err;
				json_object_object_add(jndns, "chardev", jobj);
			}
		}
	} else if (ndctl_namespace_get_type(ndns) != ND_DEVICE_NAMESPACE_IO) {
		ndctl_namespace_get_uuid(ndns, uuid);
		uuid_unparse(uuid, buf);
		jobj = json_object_new_string(buf);
		if (!jobj)
			goto err;
		json_object_object_add(jndns, "uuid", jobj);
		bdev = ndctl_namespace_get_block_device(ndns);
	} else
		bdev = ndctl_namespace_get_block_device(ndns);

	if (bdev && bdev[0]) {
		jobj = json_object_new_string(bdev);
		if (!jobj)
			goto err;
		json_object_object_add(jndns, "blockdev", jobj);
	}

	if (!ndctl_namespace_is_active(ndns)) {
		jobj = json_object_new_string("disabled");
		if (!jobj)
			goto err;
		json_object_object_add(jndns, "state", jobj);
	}

	name = ndctl_namespace_get_alt_name(ndns);
	if (name && name[0]) {
		jobj = json_object_new_string(name);
		if (!jobj)
			goto err;
		json_object_object_add(jndns, "name", jobj);
	}

	numa = ndctl_namespace_get_numa_node(ndns);
	if (numa >= 0) {
		jobj = json_object_new_int(numa);
		if (jobj)
			json_object_object_add(jndns, "numa_node", jobj);
	}

	if (pfn)
		jbbs = util_pfn_badblocks_to_json(pfn, &bb_count, flags);
	else if (dax)
		jbbs = util_dax_badblocks_to_json(dax, &bb_count, flags);
	else if (btt)
		util_btt_badblocks_to_json(btt, &bb_count);
	else
		jbbs = util_region_badblocks_to_json(
				ndctl_namespace_get_region(ndns), &bb_count,
				flags);

	if (bb_count) {
		jobj = json_object_new_int(bb_count);
		if (!jobj) {
			json_object_put(jbbs);
			goto err;
		}
		json_object_object_add(jndns, "badblock_count", jobj);
	}

	if ((flags & UTIL_JSON_MEDIA_ERRORS) && jbbs)
		json_object_object_add(jndns, "badblocks", jbbs);

	return jndns;
 err:
	json_object_put(jndns);
	return NULL;
}

struct json_object *util_mapping_to_json(struct ndctl_mapping *mapping,
		unsigned long flags)
{
	struct json_object *jmapping = json_object_new_object();
	struct ndctl_dimm *dimm = ndctl_mapping_get_dimm(mapping);
	struct json_object *jobj;
	int position;

	if (!jmapping)
		return NULL;

	jobj = json_object_new_string(ndctl_dimm_get_devname(dimm));
	if (!jobj)
		goto err;
	json_object_object_add(jmapping, "dimm", jobj);

	jobj = util_json_object_hex(ndctl_mapping_get_offset(mapping), flags);
	if (!jobj)
		goto err;
	json_object_object_add(jmapping, "offset", jobj);

	jobj = util_json_object_hex(ndctl_mapping_get_length(mapping), flags);
	if (!jobj)
		goto err;
	json_object_object_add(jmapping, "length", jobj);

	position = ndctl_mapping_get_position(mapping);
	if (position >= 0) {
		jobj = json_object_new_int(position);
		if (!jobj)
			goto err;
		json_object_object_add(jmapping, "position", jobj);
	}

	return jmapping;
 err:
	json_object_put(jmapping);
	return NULL;
}

struct json_object *util_badblock_rec_to_json(u64 block, u64 count,
		unsigned long flags)
{
	struct json_object *jerr = json_object_new_object();
	struct json_object *jobj;

	if (!jerr)
		return NULL;

	jobj = util_json_object_hex(block, flags);
	if (!jobj)
		goto err;
	json_object_object_add(jerr, "block", jobj);

	jobj = util_json_object_hex(count, flags);
	if (!jobj)
		goto err;
	json_object_object_add(jerr, "count", jobj);

	return jerr;
 err:
	json_object_put(jerr);
	return NULL;
}
