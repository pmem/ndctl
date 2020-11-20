// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2017-2020 Intel Corporation. All rights reserved.
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <ndctl/libndctl.h>

static void dimm_listen(struct ndctl_bus *bus)
{
	struct ndctl_dimm *dimm, **dimms;
	int count = 0, maxfd = -1, i, rc;
	struct pollfd *poll_ents, *p;
	char buf;

	ndctl_dimm_foreach(bus, dimm) {
		int fd = ndctl_dimm_get_health_eventfd(dimm);

		if (fd > maxfd)
			maxfd = fd;
		count++;
	}

	if (!count) {
		fprintf(stderr, "no dimms on bus: %s\n",
				ndctl_bus_get_provider(bus));
		return;
	}

	poll_ents = calloc(count, sizeof(struct pollfd));
	dimms = calloc(maxfd + 1, sizeof(struct ndctl_dimm *));

	if (!poll_ents)
		goto out;
	if (!dimms)
		goto out;

	i = 0;
	ndctl_dimm_foreach(bus, dimm) {
		int fd = ndctl_dimm_get_health_eventfd(dimm);

		p = &poll_ents[i++];
		p->fd = fd;
		dimms[fd] = dimm;
		if (i > count) {
			fprintf(stderr, "dimm count changed!?\n");
			goto out;
		}
	}

retry:
	for (i = 0; i < count; i++) {
		p = &poll_ents[i];
		dimm = dimms[p->fd];
		if (pread(p->fd, &buf, 1, 0) != 1) {
			fprintf(stderr, "%s: failed to read\n",
					ndctl_dimm_get_devname(dimm));
			goto out;
		}
		if (p->revents)
			fprintf(stderr, "%s: smart event: %d\n",
					ndctl_dimm_get_devname(dimm),
					p->revents);
		p->revents = 0;
	}

	rc = poll(poll_ents, count, -1);
	if (rc <= 0) {
		fprintf(stderr, "failed to poll\n");
		goto out;
	}
	goto retry;

out:
	free(poll_ents);
	free(dimms);
}

int main(int argc, char *argv[])
{
	struct ndctl_ctx *ctx;
	struct ndctl_bus *bus;
	int rc = EXIT_FAILURE;
	const char *provider;

	rc = ndctl_new(&ctx);
	if (rc < 0)
		return EXIT_FAILURE;

	if (argc != 2) {
		fprintf(stderr, "usage: smart-notify <nvdimm-bus-provider>\n");
		goto out;
	}

	provider = argv[1];
	bus = ndctl_bus_get_by_provider(ctx, provider);
	if (!bus) {
		fprintf(stderr, "smart-notify: unable to find bus (%s)\n",
				provider);
		goto out;
	}

	rc = EXIT_SUCCESS;
	dimm_listen(bus);
out:
	ndctl_unref(ctx);
	return rc;
}
