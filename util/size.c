#include <stdlib.h>
#include <limits.h>
#include <util/size.h>

unsigned long long parse_size64(const char *str)
{
	unsigned long long val, check;
	char *end;

	val = strtoull(str, &end, 0);
	if (val == ULLONG_MAX)
		return val;
	check = val;
	switch (*end) {
		case 'k':
		case 'K':
			val *= SZ_1K;
			end++;
			break;
		case 'm':
		case 'M':
			val *= SZ_1M;
			end++;
			break;
		case 'g':
		case 'G':
			val *= SZ_1G;
			end++;
			break;
		case 't':
		case 'T':
			val *= SZ_1T;
			end++;
			break;
		default:
			break;
	}

	if (val < check || *end != '\0')
		val = ULLONG_MAX;
	return val;
}
