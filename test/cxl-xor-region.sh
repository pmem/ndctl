#!/bin/bash
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2022 Intel Corporation. All rights reserved.

. $(dirname $0)/common

rc=77

set -ex

trap 'err $LINENO' ERR

check_prereq "jq"

modprobe -r cxl_test
modprobe cxl_test interleave_arithmetic=1
udevadm settle
rc=1

# THEORY OF OPERATION: Create x1,2,3,4 regions to exercise the XOR math
# option of the CXL driver. As with other cxl_test tests, changes to the
# CXL topology in tools/testing/cxl/test/cxl.c may require an update here.

create_and_destroy_region()
{
	region=$($CXL create-region -d "$decoder" -m "$memdevs" |
		jq -r ".region")

	if [[ ! $region ]]; then
		echo "create-region failed for $decoder"
		err "$LINENO"
	fi

	$CXL destroy-region -f -b cxl_test "$region"
}

setup_x1()
{
	# Find an x1 decoder
	decoder=$($CXL list -b cxl_test -D -d root | jq -r ".[] |
		select(.pmem_capable == true) |
		select(.nr_targets == 1) |
		.decoder")

	# Find a memdev for this host-bridge
	port_dev0=$($CXL list -T -d "$decoder" | jq -r ".[] |
		.targets | .[] | select(.position == 0) | .target")
	mem0=$($CXL list -M -p "$port_dev0" | jq -r ".[0].memdev")
	memdevs="$mem0"
}

setup_x2()
{
	# Find an x2 decoder
	decoder=$($CXL list -b cxl_test -D -d root | jq -r ".[] |
		select(.pmem_capable == true) |
		select(.nr_targets == 2) |
		.decoder")

	# Find a memdev for each host-bridge interleave position
	port_dev0=$($CXL list -T -d "$decoder" | jq -r ".[] |
		.targets | .[] | select(.position == 0) | .target")
	port_dev1=$($CXL list -T -d "$decoder" | jq -r ".[] |
		.targets | .[] | select(.position == 1) | .target")
	mem0=$($CXL list -M -p "$port_dev0" | jq -r ".[0].memdev")
	mem1=$($CXL list -M -p "$port_dev1" | jq -r ".[0].memdev")
	memdevs="$mem0 $mem1"
}

setup_x4()
{
	# find an x2 decoder
	decoder=$($CXL list -b cxl_test -D -d root | jq -r ".[] |
		select(.pmem_capable == true) |
		select(.nr_targets == 2) |
		.decoder")

	# Find a memdev for each host-bridge interleave position
	port_dev0=$($CXL list -T -d "$decoder" | jq -r ".[] |
		.targets | .[] | select(.position == 0) | .target")
	port_dev1=$($CXL list -T -d "$decoder" | jq -r ".[] |
		.targets | .[] | select(.position == 1) | .target")
	mem0=$($CXL list -M -p "$port_dev0" | jq -r ".[0].memdev")
	mem1=$($CXL list -M -p "$port_dev1" | jq -r ".[0].memdev")
	mem2=$($CXL list -M -p "$port_dev0" | jq -r ".[1].memdev")
	mem3=$($CXL list -M -p "$port_dev1" | jq -r ".[1].memdev")
	memdevs="$mem0 $mem1 $mem2 $mem3"
}

setup_x1
create_and_destroy_region
setup_x2
create_and_destroy_region
setup_x4
create_and_destroy_region

modprobe -r cxl_test
