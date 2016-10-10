#ifndef _NDCTL_SIZE_H_
#define _NDCTL_SIZE_H_

#define SZ_1K     0x00000400
#define SZ_1M     0x00100000
#define SZ_2M     0x00200000
#define SZ_4M     0x00400000
#define SZ_16M    0x01000000
#define SZ_64M    0x04000000
#define SZ_1G     0x40000000
#define SZ_1T 0x10000000000ULL

unsigned long long parse_size64(const char *str);

#endif /* _NDCTL_SIZE_H_ */
