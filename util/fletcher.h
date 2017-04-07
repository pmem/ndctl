#ifndef _NDCTL_FLETCHER_H_
#define _NDCTL_FLETCHER_H_

#include <ccan/short_types/short_types.h>

u64 fletcher64(void *addr, size_t len, bool le);

#endif /* _NDCTL_FLETCHER_H_ */
