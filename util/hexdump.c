// SPDX-License-Identifier: GPL-2.0
/* Copyright (C) 2015-2021 Intel Corporation. All rights reserved. */
#include <stdio.h>
#include <util/hexdump.h>

static void print_separator(int len)
{
	int i;

	for (i = 0; i < len; i++)
		fprintf(stderr, "-");
	fprintf(stderr, "\n");
}

void hex_dump_buf(unsigned char *buf, int size)
{
	int i;
	const int grp = 4;  /* Number of bytes in a group */
	const int wid = 16; /* Bytes per line. Should be a multiple of grp */
	char ascii[wid + 1];

	/* Generate header */
	print_separator((wid * 4) + (wid / grp) + 12);

	fprintf(stderr, "Offset    ");
	for (i = 0; i < wid; i++) {
		if (i % grp == 0) fprintf(stderr, " ");
		fprintf(stderr, "%02x ", i);
	}
	fprintf(stderr, "  Ascii\n");

	print_separator((wid * 4) + (wid / grp) + 12);

	/* Generate hex dump */
	for (i = 0; i < size; i++) {
		if (i % wid == 0) fprintf(stderr, "%08x  ", i);
		ascii[i % wid] =
		    ((buf[i] >= ' ') && (buf[i] <= '~')) ? buf[i] : '.';
		if (i % grp == 0) fprintf(stderr, " ");
		fprintf(stderr, "%02x ", buf[i]);
		if ((i == size - 1) && (size % wid != 0)) {
			int j;
			int done = size % wid;
			int grps_done = (done / grp) + ((done % grp) ? 1 : 0);
			int spaces = wid / grp - grps_done + ((wid - done) * 3);

			for (j = 0; j < spaces; j++) fprintf(stderr, " ");
		}
		if ((i % wid == wid - 1) || (i == size - 1))
			fprintf(stderr, "  %.*s\n", (i % wid) + 1, ascii);
	}
	print_separator((wid * 4) + (wid / grp) + 12);
}
