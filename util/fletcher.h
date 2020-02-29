#ifndef _NDCTL_FLETCHER_H_
#define _NDCTL_FLETCHER_H_

#include <stdbool.h>
#include <ccan/endian/endian.h>
#include <ccan/short_types/short_types.h>

/*
 * Note, fletcher64() is copied from drivers/nvdimm/label.c in the Linux kernel
 */
static inline u64 fletcher64(void *addr, size_t len, bool le)
{
	u32 *buf = addr;
	u32 lo32 = 0;
	u64 hi32 = 0;
	size_t i;

	for (i = 0; i < len / sizeof(u32); i++) {
		lo32 += le ? le32_to_cpu((le32) buf[i]) : buf[i];
		hi32 += lo32;
	}

	return hi32 << 32 | lo32;
}

#endif /* _NDCTL_FLETCHER_H_ */
