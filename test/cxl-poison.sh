#!/bin/bash
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2023 Intel Corporation. All rights reserved.

. "$(dirname "$0")"/common

rc=77

set -ex

trap 'err $LINENO' ERR

check_prereq "jq"

modprobe -r cxl_test
modprobe cxl_test

rc=1

# THEORY OF OPERATION: Exercise cxl-cli and cxl driver ability to
# inject, clear, and get the poison list. Do it by memdev and by region.

find_memdev()
{
	readarray -t capable_mems < <("$CXL" list -b "$CXL_TEST_BUS" -M |
		jq -r ".[] | select(.pmem_size != null) |
		select(.ram_size != null) | .memdev")

	if [ ${#capable_mems[@]} == 0 ]; then
		echo "no memdevs found for test"
		err "$LINENO"
	fi

	memdev=${capable_mems[0]}
}

create_x2_region()
{
	# Find an x2 decoder
	decoder="$($CXL list -b "$CXL_TEST_BUS" -D -d root | jq -r ".[] |
		select(.pmem_capable == true) |
		select(.nr_targets == 2) |
		.decoder")"

	# Find a memdev for each host-bridge interleave position
	port_dev0="$($CXL list -T -d "$decoder" | jq -r ".[] |
		.targets | .[] | select(.position == 0) | .target")"
	port_dev1="$($CXL list -T -d "$decoder" | jq -r ".[] |
		.targets | .[] | select(.position == 1) | .target")"
	mem0="$($CXL list -M -p "$port_dev0" | jq -r ".[0].memdev")"
	mem1="$($CXL list -M -p "$port_dev1" | jq -r ".[0].memdev")"

	region="$($CXL create-region -d "$decoder" -m "$mem0" "$mem1" |
		jq -r ".region")"
	if [[ ! $region ]]; then
		echo "create-region failed for $decoder"
		err "$LINENO"
	fi
	echo "$region"
}

# When cxl-cli support for inject and clear arrives, replace
# the writes to /sys/kernel/debug with the new cxl commands.

inject_poison_sysfs()
{
	memdev="$1"
	addr="$2"

	echo "$addr" > /sys/kernel/debug/cxl/"$memdev"/inject_poison
}

clear_poison_sysfs()
{
	memdev="$1"
	addr="$2"

	echo "$addr" > /sys/kernel/debug/cxl/"$memdev"/clear_poison
}

validate_poison_found()
{
	list_by="$1"
	nr_expect="$2"

	poison_list="$($CXL list "$list_by" --media-errors |
		jq -r '.[].media_errors')"
	if [[ ! $poison_list ]]; then
		nr_found=0
	else
		nr_found=$(jq "length" <<< "$poison_list")
	fi
	if [ "$nr_found" -ne "$nr_expect" ]; then
		echo "$nr_expect poison records expected, $nr_found found"
		err "$LINENO"
	fi
}

test_poison_by_memdev()
{
	find_memdev
	inject_poison_sysfs "$memdev" "0x40000000"
	inject_poison_sysfs "$memdev" "0x40001000"
	inject_poison_sysfs "$memdev" "0x600"
	inject_poison_sysfs "$memdev" "0x0"
	validate_poison_found "-m $memdev" 4

	clear_poison_sysfs "$memdev" "0x40000000"
	clear_poison_sysfs "$memdev" "0x40001000"
	clear_poison_sysfs "$memdev" "0x600"
	clear_poison_sysfs "$memdev" "0x0"
	validate_poison_found "-m $memdev" 0
}

test_poison_by_region()
{
	create_x2_region
	inject_poison_sysfs "$mem0" "0x40000000"
	inject_poison_sysfs "$mem1" "0x40000000"
	validate_poison_found "-r $region" 2

	clear_poison_sysfs "$mem0" "0x40000000"
	clear_poison_sysfs "$mem1" "0x40000000"
	validate_poison_found "-r $region" 0
}

# Turn tracing on. Note that 'cxl list --media-errors' toggles the tracing.
# Turning it on here allows the test user to also view inject and clear
# trace events.
echo 1 > /sys/kernel/tracing/events/cxl/cxl_poison/enable

test_poison_by_memdev
test_poison_by_region

check_dmesg "$LINENO"

modprobe -r cxl-test
