#include <limits.h>
#include <util/json.h>
#include <util/filter.h>
#include <uuid/uuid.h>
#include <json-c/json.h>
#include <ndctl/libndctl.h>
#include <daxctl/libdaxctl.h>
#include <ccan/array_size/array_size.h>

#ifdef HAVE_NDCTL_H
#include <linux/ndctl.h>
#else
#include <ndctl.h>
#endif

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

struct json_object *util_dimm_to_json(struct ndctl_dimm *dimm)
{
	struct json_object *jdimm = json_object_new_object();
	const char *id = ndctl_dimm_get_unique_id(dimm);
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

struct json_object *util_daxctl_dev_to_json(struct daxctl_dev *dev)
{
	const char *devname = daxctl_dev_get_devname(dev);
	struct json_object *jdev, *jobj;

	jdev = json_object_new_object();
	if (!devname || !jdev)
		return NULL;

	jobj = json_object_new_string(devname);
	if (jobj)
		json_object_object_add(jdev, "chardev", jobj);

	jobj = json_object_new_int64(daxctl_dev_get_size(dev));
	if (jobj)
		json_object_object_add(jdev, "size", jobj);

	return jdev;
}

struct json_object *util_daxctl_devs_to_list(struct daxctl_region *region,
		struct json_object *jdevs, const char *ident, bool include_idle)
{
	struct daxctl_dev *dev;

	daxctl_dev_foreach(region, dev) {
		struct json_object *jdev;

		if (!util_daxctl_dev_filter(dev, ident))
			continue;

		if (!include_idle && !daxctl_dev_get_size(dev))
			continue;

		if (!jdevs) {
			jdevs = json_object_new_array();
			if (!jdevs)
				return NULL;
		}

		jdev = util_daxctl_dev_to_json(dev);
		if (!jdev) {
			json_object_put(jdevs);
			return NULL;
		}

		json_object_array_add(jdevs, jdev);
	}

	return jdevs;
}

struct json_object *util_daxctl_region_to_json(struct daxctl_region *region,
		bool include_devs, const char *ident, bool include_idle)
{
	unsigned long align;
	struct json_object *jregion, *jobj;
	unsigned long long available_size, size;

	jregion = json_object_new_object();
	if (!jregion)
		return NULL;

	jobj = json_object_new_int(daxctl_region_get_id(region));
	if (!jobj)
		goto err;
	json_object_object_add(jregion, "id", jobj);

	size = daxctl_region_get_size(region);
	if (size < ULLONG_MAX) {
		jobj = json_object_new_int64(size);
		if (!jobj)
			goto err;
		json_object_object_add(jregion, "size", jobj);
	}

	available_size = daxctl_region_get_available_size(region);
	if (available_size) {
		jobj = json_object_new_int64(available_size);
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

	if (!include_devs)
		return jregion;

	jobj = util_daxctl_devs_to_list(region, NULL, ident, include_idle);
	if (jobj)
		json_object_object_add(jregion, "devices", jobj);

	return jregion;
 err:
	json_object_put(jregion);
	return NULL;
}

struct json_object *util_region_badblocks_to_json(struct ndctl_region *region,
		bool include_media_errors, unsigned int *bb_count)
{
	struct json_object *jbb = NULL, *jbbs = NULL, *jobj;
	struct badblock *bb;
	int bbs = 0;

	if (include_media_errors) {
		jbbs = json_object_new_array();
		if (!jbbs)
			return NULL;
	}

	ndctl_region_badblock_foreach(region, bb) {
		if (include_media_errors) {
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

			json_object_array_add(jbbs, jbb);
		}

		bbs += bb->len;
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
		bool include_media_errors, unsigned int *bb_count)
{
	struct json_object *jbb = NULL, *jbbs = NULL, *jobj;
	unsigned long long region_begin, dev_end, offset;
	unsigned int len, bbs = 0;
	struct badblock *bb;

	region_begin = ndctl_region_get_resource(region);
	if (region_begin == ULLONG_MAX)
		return NULL;

	dev_end = dev_begin + dev_size - 1;

	if (include_media_errors) {
		jbbs = json_object_new_array();
		if (!jbbs)
			return NULL;
	}

	ndctl_region_badblock_foreach(region, bb) {
		unsigned long long bb_begin, bb_end, begin, end;

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

		if (include_media_errors) {
			/* add to json */
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

			json_object_array_add(jbbs, jbb);
		}
		bbs += len;
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

struct json_object *util_pfn_badblocks_to_json(struct ndctl_pfn *pfn,
		bool include_media_errors, unsigned int *bb_count)
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
			include_media_errors, bb_count);
}

struct json_object *util_btt_badblocks_to_json(struct ndctl_btt *btt,
		bool include_media_errors, unsigned int *bb_count)
{
	struct ndctl_region *region = ndctl_btt_get_region(btt);
	struct ndctl_namespace *ndns = ndctl_btt_get_namespace(btt);
	unsigned long long btt_begin, btt_size;

	btt_begin = ndctl_namespace_get_resource(ndns);
	if (btt_begin == ULLONG_MAX)
		return NULL;

	btt_size = ndctl_btt_get_size(btt);
	if (btt_size == ULLONG_MAX)
		return NULL;

	return dev_badblocks_to_json(region, btt_begin, btt_size,
			include_media_errors, bb_count);
}

struct json_object *util_dax_badblocks_to_json(struct ndctl_dax *dax,
		bool include_media_errors, unsigned int *bb_count)
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
			include_media_errors, bb_count);
}

struct json_object *util_namespace_to_json(struct ndctl_namespace *ndns,
		bool include_idle, bool include_dax,
		bool include_media_errors)
{
	struct json_object *jndns = json_object_new_object();
	unsigned long long size = ULLONG_MAX;
	enum ndctl_namespace_mode mode;
	struct json_object *jobj, *jobj2;
	const char *bdev = NULL;
	struct ndctl_btt *btt;
	struct ndctl_pfn *pfn;
	struct ndctl_dax *dax;
	char buf[40];
	uuid_t uuid;
	unsigned int bb_count;

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
		jobj = json_object_new_int64(size);
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

		ndctl_dax_get_uuid(dax, uuid);
		uuid_unparse(uuid, buf);
		jobj = json_object_new_string(buf);
		if (!jobj)
			goto err;
		json_object_object_add(jndns, "uuid", jobj);
		if (include_dax) {
			dax_region = ndctl_dax_get_daxctl_region(dax);
			jobj = util_daxctl_region_to_json(dax_region,
					true, NULL, include_idle);
			if (jobj)
				json_object_object_add(jndns, "daxregion", jobj);
		}
	} else if (ndctl_namespace_get_type(ndns) != ND_DEVICE_NAMESPACE_IO) {
		const char *name;

		ndctl_namespace_get_uuid(ndns, uuid);
		uuid_unparse(uuid, buf);
		jobj = json_object_new_string(buf);
		if (!jobj)
			goto err;
		json_object_object_add(jndns, "uuid", jobj);

		name = ndctl_namespace_get_alt_name(ndns);
		if (name[0]) {
			jobj = json_object_new_string(name);
			if (!jobj)
				goto err;
			json_object_object_add(jndns, "name", jobj);
		}
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

	if (pfn)
		jobj2 = util_pfn_badblocks_to_json(pfn, include_media_errors,
				&bb_count);
	else if (dax)
		jobj2 = util_dax_badblocks_to_json(dax, include_media_errors,
				&bb_count);
	else if (btt) {
		jobj2 = util_btt_badblocks_to_json(btt, include_media_errors,
				&bb_count);
		/*
		 * Discard the jobj2, the badblocks for BTT is not,
		 * accurate and there will be a good method to caculate
		 * them later. We just want a bb count and not the specifics
		 * for now.
		 */
		jobj2 = NULL;
	} else {
		struct ndctl_region *region =
			ndctl_namespace_get_region(ndns);

		jobj2 = util_region_badblocks_to_json(region,
				include_media_errors, &bb_count);
	}

	jobj = json_object_new_int(bb_count);
	if (!jobj) {
		json_object_put(jobj2);
		goto err;
	}

	json_object_object_add(jndns, "badblock_count", jobj);
	if (include_media_errors && jobj2)
			json_object_object_add(jndns, "badblocks", jobj2);

	return jndns;
 err:
	json_object_put(jndns);
	return NULL;
}

struct json_object *util_mapping_to_json(struct ndctl_mapping *mapping)
{
	struct json_object *jmapping = json_object_new_object();
	struct ndctl_dimm *dimm = ndctl_mapping_get_dimm(mapping);
	struct json_object *jobj;

	if (!jmapping)
		return NULL;

	jobj = json_object_new_string(ndctl_dimm_get_devname(dimm));
	if (!jobj)
		goto err;
	json_object_object_add(jmapping, "dimm", jobj);

	jobj = json_object_new_int64(ndctl_mapping_get_offset(mapping));
	if (!jobj)
		goto err;
	json_object_object_add(jmapping, "offset", jobj);

	jobj = json_object_new_int64(ndctl_mapping_get_length(mapping));
	if (!jobj)
		goto err;
	json_object_object_add(jmapping, "length", jobj);

	return jmapping;
 err:
	json_object_put(jmapping);
	return NULL;
}
