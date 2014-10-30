#include <linux/ioctl.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <asm/types.h>
#include <linux/ndctl.h>
#include <ndctl/libndctl.h>
#include <libkmod.h>
#include <syslog.h>
#include "cr_ioctl.h"
#include <errno.h>

struct identify_dimm_cmd {
	__u32 nfit_handle;
	__u32 in_length; 			// 8
	struct {
		__u8 data_format_revision; 	// 1
		__u8 opcode;		   	// 1
		__u8 sub_opcode;		// 0
		__u8 flags;			// 0
		__u32 reserved;
		__u8 in_buf[0];
	} in;

	__u32 status;
	__u32 out_length; 			// 128
	__u8  out_buf[128];
};

static int send_id_dimm(int fd)
{
	struct identify_dimm_cmd cmd;
	struct cr_pt_payload_identify_dimm dimm_id_payload;
	int ret;

	memset(&cmd, 0, sizeof(cmd));

	cmd.in_length = sizeof(cmd.in);
	cmd.in.data_format_revision 	= 1;
	cmd.in.opcode 			= 1;
	cmd.in.sub_opcode 		= 0;
	cmd.in.flags	 		= 0;

	cmd.out_length = 128;

	ret = ioctl(fd, NFIT_IOCTL_VENDOR, &cmd);
	if (ret) {
		perror("ioctl");
		exit(errno);
	}

	memcpy(&dimm_id_payload, cmd.out_buf, sizeof(dimm_id_payload));
	/*
	printf("Identify DIMM\n"
		"VendorID: %#hx\n"
		"DeviceID: %#hx\n"
		"RevisionID: %#hx\n"
		"InterfaceCode: %#hx\n"
		"FirmwareRevision(BCD): %c%c%c%c%c\n"
		"API Version(BCD): %#hhx\n"
		"Feature SW Mask: %#hhx\n"
		"NumberBlockWindows: %#hx\n"
		"NumberWFAs: %#hhx\n"
		"BlockCTRL_Offset: %#x\n"
		"RawCapacity: %llu GB\n"
		"Manufacturer: %s\n"
		"Serial Number: %s\n"
		"Model Number: %s\n",
		dimm_id_payload.vendor_id,
		dimm_id_payload.device_id,
		dimm_id_payload.revision_id,
		dimm_id_payload.ifc,
		dimm_id_payload.fwr[0],
		dimm_id_payload.fwr[1],
		dimm_id_payload.fwr[2],
		dimm_id_payload.fwr[3],
		dimm_id_payload.fwr[4],
		dimm_id_payload.api_ver,
		dimm_id_payload.fswr,
		dimm_id_payload.nbw,
		dimm_id_payload.nwfa,
		dimm_id_payload.obmcr,
		dimm_id_payload.rc >> 18,
		(char *)dimm_id_payload.mf,
		(char *)dimm_id_payload.sn,
		(char *)dimm_id_payload.mn);
	*/

	// check some values to make sure things look sane
	if (dimm_id_payload.vendor_id != 0x8086 	||
	    dimm_id_payload.device_id != 0x2017 	||
	    dimm_id_payload.revision_id != 0xabcd 	||
	    dimm_id_payload.ifc != 0x1)
		return EIO;

	return 0;
}

static struct ndctl_bus *get_acpi_nfit_bus(struct ndctl_ctx *ctx)
{
	struct ndctl_bus *bus;

        ndctl_bus_foreach(ctx, bus)
		if (strcmp("ACPI.NFIT", ndctl_bus_get_provider(bus)) == 0)
			return bus;
	return NULL;
}

int main(int argc, char *argv[])
{
	int rc = -ENXIO, fd;
	struct ndctl_ctx *ctx;
	struct ndctl_bus *bus;
	struct kmod_module *mod;
	struct kmod_ctx *kmod_ctx;
	char path[50];

	rc = ndctl_new(&ctx);
	if (rc < 0)
		exit(EXIT_FAILURE);

	ndctl_set_log_priority(ctx, LOG_DEBUG);

	kmod_ctx = kmod_new(NULL, NULL);
	if (!kmod_ctx)
		goto err_kmod;

	rc = kmod_module_new_from_name(kmod_ctx, "nd_acpi", &mod);
	if (rc < 0) {
		fprintf(stderr, "failed to find nd_acpi\n");
		goto err_module;
	}

	rc = kmod_module_probe_insert_module(mod, KMOD_PROBE_APPLY_BLACKLIST,
			NULL, NULL, NULL, NULL);
	if (rc < 0) {
		fprintf(stderr, "failed to load nd_acpi\n");
		goto err_module;
	}
	rc = -ENXIO;

	bus = get_acpi_nfit_bus(ctx);
	if (!bus) {
		fprintf(stderr, "failed to find ACPI.NFIT bus\n");
		goto err_bus;
	}

	if (!ndctl_bus_is_cmd_supported(bus, NFIT_CMD_VENDOR)) {
		fprintf(stderr, "NFIT_CMD_VENDOR not supported\n");
		goto err_bus;
	}

	sprintf(path, "/dev/ndctl%d", ndctl_bus_get_id(bus));
	fd = open(path, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "failed to open %s\n", path);
		goto err_bus;
	}

	rc = send_id_dimm(fd);

	close(fd);
 err_bus:
	kmod_module_remove_module(mod, 0);
 err_module:
	kmod_unref(kmod_ctx);
 err_kmod:
	ndctl_unref(ctx);

	return rc;
}
