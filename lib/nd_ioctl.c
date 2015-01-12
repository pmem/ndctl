#include <asm/types.h>
#include <ccan/array_size/array_size.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/ioctl.h>
#include <linux/ndctl.h>
#include <ndctl/libndctl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <syslog.h>
#include <sys/mman.h>
#include <unistd.h>

#include "cr_ioctl.h"

#define CR_CMD(NAME, IN_LEN, OUT_LEN)                                         \
struct NAME {                                                                 \
	__u32 in_length;                                                      \
	struct {                                                              \
		__u8 data_format_revision;                                    \
		__u8 opcode;                                                  \
		__u8 sub_opcode;                                              \
		__u8 flags;                                                   \
		__u32 reserved;                                               \
		__u8 in_buf[IN_LEN];                                          \
	} in;                                                                 \
                                                                              \
	__u32 status;                                                         \
	__u32 out_length;                                                     \
	__u8  out_buf[OUT_LEN];                                               \
}

#define FNV_BIOS_INPUT(NAME, IN_LEN)                                          \
struct NAME {                                                                 \
	__u32 size;                                                           \
	__u32 offset;                                                         \
	__u8  buffer[IN_LEN];                                                 \
}

static int write_string(int fd, struct ndctl_dimm *dimm)
{
	const int size = 128;
	FNV_BIOS_INPUT(bios_input, size);
	CR_CMD(fnv_write_large_payload, sizeof(struct bios_input), 0);
	struct fnv_write_large_payload cmd;
	struct bios_input *bios_cmd = (struct bios_input *)cmd.in.in_buf;
	int ret;

	memset(&cmd, 0, sizeof(cmd));

	cmd.in_length = sizeof(cmd.in);
	cmd.in.data_format_revision 	= 1;
	cmd.in.opcode 			= FNV_BIOS_OPCODE;
	cmd.in.sub_opcode 		= FNV_BIOS_SUBOP_WRITE_INPUT;
	cmd.in.flags	 		= FNV_BIOS_FLAG;

	strcpy((char*)bios_cmd->buffer, "0123456789");

	bios_cmd->offset = 0;

	cmd.out_length = sizeof(cmd.out_buf);

	ret = ioctl(fd, NFIT_IOCTL_VENDOR, &cmd);
	if (ret) {
		perror(__func__);
		return -EINVAL;
	}

	bios_cmd->offset = 10;
	strcpy((char*)bios_cmd->buffer, "abcdefghij");
	ret = ioctl(fd, NFIT_IOCTL_VENDOR, &cmd);

	if (ret) {
		perror(__func__);
		return -EINVAL;
	}

	return 0;
}

static int read_string(int fd, struct ndctl_dimm *dimm)
{
	const int size = 128;
	FNV_BIOS_INPUT(bios_input, 0);
	CR_CMD(fnv_write_large_payload, sizeof(struct bios_input), size);
	struct fnv_write_large_payload cmd;
	struct bios_input *bios_cmd = (struct bios_input *)cmd.in.in_buf;
	int ret;

	memset(&cmd, 0, sizeof(cmd));

	cmd.in_length = sizeof(cmd.in);
	cmd.in.data_format_revision 	= 1;
	cmd.in.opcode 			= FNV_BIOS_OPCODE;
	cmd.in.sub_opcode 		= FNV_BIOS_SUBOP_READ_INPUT;
	cmd.in.flags	 		= FNV_BIOS_FLAG;

	cmd.out_length = sizeof(cmd.out_buf);

	// compare first substring
	bios_cmd->offset = 0;

	ret = ioctl(fd, NFIT_IOCTL_VENDOR, &cmd);
	if (ret) {
		perror(__func__);
		return -EINVAL;
	}

	if (memcmp(cmd.out_buf, "0123456", 7)) {
		fprintf(stderr, "nd_ioctl %s: compare 1 failed\n", __func__);
		return -EINVAL;
	}

	// compare second substring
	bios_cmd->offset = 7;

	ret = ioctl(fd, NFIT_IOCTL_VENDOR, &cmd);
	if (ret) {
		perror(__func__);
		return -EINVAL;
	}

	if (memcmp(cmd.out_buf, "789abcd", 7)) {
		fprintf(stderr, "nd_ioctl %s: compare 2 failed\n", __func__);
		return -EINVAL;
	}

	// compare third substring
	bios_cmd->offset = 14;

	ret = ioctl(fd, NFIT_IOCTL_VENDOR, &cmd);
	if (ret) {
		perror(__func__);
		return -EINVAL;
	}

	if (memcmp(cmd.out_buf, "efghij", 6)) {
		fprintf(stderr, "nd_ioctl %s: compare 3 failed\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static int read_large_input_payload(int fd, struct ndctl_dimm *dimm)
{
	const int one_mb = (1<<20) - 1024;
	FNV_BIOS_INPUT(bios_input, 0);
	CR_CMD(fnv_write_large_payload, sizeof(struct bios_input), one_mb);
	struct fnv_write_large_payload cmd;
	struct bios_input *bios_cmd = (struct bios_input *)cmd.in.in_buf;
	int ret;

	memset(&cmd, 0, sizeof(cmd));

	cmd.in_length = sizeof(cmd.in);
	cmd.in.data_format_revision 	= 1;
	cmd.in.opcode 			= FNV_BIOS_OPCODE;
	cmd.in.sub_opcode 		= FNV_BIOS_SUBOP_READ_INPUT;
	cmd.in.flags	 		= FNV_BIOS_FLAG;

	bios_cmd->offset = 1024;

	cmd.out_length = sizeof(cmd.out_buf);

	ret = ioctl(fd, NFIT_IOCTL_VENDOR, &cmd);
	if (ret) {
		perror(__func__);
		return -EINVAL;
	}

	return 0;
}

static int read_large_output_payload(int fd, struct ndctl_dimm *dimm)
{
	const int one_mb = (1<<20);
	FNV_BIOS_INPUT(bios_input, 0);
	CR_CMD(fnv_write_large_payload, sizeof(struct bios_input), one_mb);
	struct fnv_write_large_payload cmd;
	struct bios_input *bios_cmd = (struct bios_input *)cmd.in.in_buf;
	int ret;

	memset(&cmd, 0, sizeof(cmd));

	cmd.in_length = sizeof(cmd.in);
	cmd.in.data_format_revision 	= 1;
	cmd.in.opcode 			= FNV_BIOS_OPCODE;
	cmd.in.sub_opcode 		= FNV_BIOS_SUBOP_READ_OUTPUT;
	cmd.in.flags	 		= FNV_BIOS_FLAG;

	bios_cmd->offset = 0;

	cmd.out_length = sizeof(cmd.out_buf);

	ret = ioctl(fd, NFIT_IOCTL_VENDOR, &cmd);
	if (ret) {
		perror(__func__);
		return -EINVAL;
	}

	return 0;
}

static int write_large_payload(int fd, struct ndctl_dimm *dimm)
{
	const int one_mb = (1<<20);
	FNV_BIOS_INPUT(bios_input, one_mb);
	CR_CMD(fnv_write_large_payload, sizeof(struct bios_input), 0);
	struct fnv_write_large_payload cmd;
	struct bios_input *bios_cmd = (struct bios_input *)cmd.in.in_buf;
	int ret;

	memset(&cmd, 0, sizeof(cmd));

	cmd.in_length = sizeof(cmd.in);
	cmd.in.data_format_revision 	= 1;
	cmd.in.opcode 			= FNV_BIOS_OPCODE;
	cmd.in.sub_opcode 		= FNV_BIOS_SUBOP_WRITE_INPUT;
	cmd.in.flags	 		= FNV_BIOS_FLAG;

	bios_cmd->offset = 0;

	cmd.out_length = sizeof(cmd.out_buf);

	ret = ioctl(fd, NFIT_IOCTL_VENDOR, &cmd);
	if (ret) {
		perror(__func__);
		return -EINVAL;
	}

	return 0;
}

static int send_get_payload_size(int fd, struct ndctl_dimm *dimm)
{
	CR_CMD(get_payload_size, 0, sizeof(struct fnv_bios_get_size));
	struct get_payload_size cmd;
	int ret;
	const unsigned int one_mb = (1<<20);

	memset(&cmd, 0, sizeof(cmd));

	cmd.in_length = sizeof(cmd.in);
	cmd.in.data_format_revision 	= 1;
	cmd.in.opcode 			= FNV_BIOS_OPCODE;
	cmd.in.sub_opcode 		= FNV_BIOS_SUBOP_GET_SIZE;
	cmd.in.flags	 		= FNV_BIOS_FLAG;

	cmd.out_length = sizeof(cmd.out_buf);

	ret = ioctl(fd, NFIT_IOCTL_VENDOR, &cmd);
	if (ret) {
		perror(__func__);
		return -EINVAL;
	}

	struct fnv_bios_get_size *get_size = (struct fnv_bios_get_size *) &cmd.out_buf;

	if (get_size->input_size  != one_mb ||
	    get_size->output_size != one_mb ||
	    get_size->rw_size     != one_mb ) {
		printf("bad payload sizes reported - input:%d output:%d rw:%d\n",
				get_size->input_size, get_size->output_size,
				get_size->rw_size);
		return -EINVAL;
	}

	return 0;
}

static int send_bad_opcode(int fd, struct ndctl_dimm *dimm)
{
	CR_CMD(identify_dimm_cmd, 0, 128);
	struct identify_dimm_cmd cmd;
	int ret;

	memset(&cmd, 0, sizeof(cmd));

	cmd.in_length = sizeof(cmd.in);
	cmd.in.data_format_revision 	= 1;
	cmd.in.opcode 			= 100;
	cmd.in.sub_opcode 		= 0;
	cmd.in.flags	 		= 0;

	cmd.out_length = sizeof(cmd.out_buf);

	ret = ioctl(fd, NFIT_IOCTL_VENDOR, &cmd);

	/* 0x40004 means "extended status, bad opcode" */
	if (ret || cmd.status != 0x40004) {
		fprintf(stderr, "nd_ioctl %s: unexpected status %x\n",
				__func__, cmd.status);
		return -EINVAL;
	}

	return 0;
}

static int send_too_large(int fd, struct ndctl_dimm *dimm)
{
	CR_CMD(double_payload_cmd, 0, 256); // meant to cause an error
	struct double_payload_cmd cmd;
	int ret;

	memset(&cmd, 0, sizeof(cmd));

	cmd.in_length = sizeof(cmd.in);
	cmd.in.data_format_revision 	= 1;
	cmd.in.opcode 			= 1;
	cmd.in.sub_opcode 		= 0;
	cmd.in.flags	 		= 0;

	cmd.out_length = sizeof(cmd.out_buf);

	ret = ioctl(fd, NFIT_IOCTL_VENDOR, &cmd);
	if (!ret || errno != EINVAL) {
		fprintf(stderr, "nd_ioctl %s: unexpected errno %d\n",
				__func__, errno);
		return -EINVAL;
	}

	return 0;
}

static int send_id_dimm(int fd, struct ndctl_dimm *dimm)
{
	CR_CMD(identify_dimm_cmd, 0, 128);
	struct identify_dimm_cmd cmd;
	struct cr_pt_payload_identify_dimm dimm_id_payload;
	int ret;

	memset(&cmd, 0, sizeof(cmd));

	cmd.in_length = sizeof(cmd.in);
	cmd.in.data_format_revision 	= 1;
	cmd.in.opcode 			= 1;
	cmd.in.sub_opcode 		= 0;
	cmd.in.flags	 		= 0;

	cmd.out_length = sizeof(cmd.out_buf);

	ret = ioctl(fd, NFIT_IOCTL_VENDOR, &cmd);
	if (ret) {
		perror(__func__);
		return -EINVAL;
	}

	memcpy(&dimm_id_payload, cmd.out_buf, sizeof(dimm_id_payload));

	// check some values to make sure things look sane
	if (dimm_id_payload.vendor_id != 0x8086 	||
	    dimm_id_payload.device_id != 0x2017 	||
	    dimm_id_payload.revision_id != 0xabcd 	||
	    dimm_id_payload.ifc != 0x1) {
		fprintf(stderr, "nd_ioctl %s: bad payload\n", __func__);
		return -EINVAL;
	}

	return 0;
}

/* FIXME kill the fd parameter, and use the library helpers */
typedef int (*do_test_fn)(int fd, struct ndctl_dimm *dimm);
static do_test_fn do_test[] = {
	send_id_dimm,
	send_bad_opcode,
	send_too_large,
	send_get_payload_size,
	write_large_payload,
	read_large_output_payload,
	read_large_input_payload,
	write_string,
	read_string,
};

static int test_bus(struct ndctl_bus *bus)
{
	int fd;
	char path[50];
	unsigned int i;
	struct ndctl_dimm *dimm = ndctl_dimm_get_first(bus);

	if (!dimm) {
		fprintf(stderr, "failed to find a dimm, skipping bus: %s\n",
				ndctl_bus_get_provider(bus));
		return 0;
	}

	if (!ndctl_dimm_is_cmd_supported(dimm, NFIT_CMD_VENDOR)) {
		fprintf(stderr, "NFIT_CMD_VENDOR not supported, skipping bus: %s\n",
				ndctl_bus_get_provider(bus));
		return 0;
	}

	sprintf(path, "/dev/nvdimm%d", ndctl_dimm_get_id(dimm));
	fd = open(path, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "failed to open %s\n", path);
		return -ENXIO;
	}

	for (i = 0; i < ARRAY_SIZE(do_test); i++) {
		int err = do_test[i](fd, dimm);

		if (err < 0) {
			fprintf(stderr, "ND IOCTL test %d failed: %d\n", i, err);
			break;
		}
	}

	close(fd);

	if (i >= ARRAY_SIZE(do_test))
		return 0;
	return -ENXIO;
}

int main(int argc, char *argv[])
{
	int rc = -ENXIO;
	struct ndctl_ctx *ctx;
	struct ndctl_bus *bus;
	int result = EXIT_SUCCESS;

	rc = ndctl_new(&ctx);
	if (rc < 0)
		exit(EXIT_FAILURE);

	ndctl_set_log_priority(ctx, LOG_DEBUG);

	/* assumes all busses to be tested already have their driver loaded */
	ndctl_bus_foreach(ctx, bus) {
		rc = test_bus(bus);
		if (rc)
			result = EXIT_FAILURE;
	}

	ndctl_unref(ctx);

	return result;
}
