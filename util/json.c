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

struct json_object *util_namespace_to_json(struct ndctl_namespace *ndns,
		bool include_idle, bool include_dax)
{
	struct json_object *jndns = json_object_new_object();
	unsigned long long size = ULLONG_MAX;
	enum ndctl_namespace_mode mode;
	struct json_object *jobj;
	const char *bdev = NULL;
	struct ndctl_btt *btt;
	struct ndctl_pfn *pfn;
	struct ndctl_dax *dax;
	char buf[40];
	uuid_t uuid;

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
