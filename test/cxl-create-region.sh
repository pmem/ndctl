#!/bin/bash
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2022 Intel Corporation. All rights reserved.

. $(dirname $0)/common

rc=77

set -ex

trap 'err $LINENO' ERR

check_prereq "jq"

load_cxl_test()
{
	modprobe -r cxl_test
	modprobe cxl_test "$@"
	readarray -t mems < <("$CXL" list -b cxl_test -M | jq -r '.[].memdev')
}

load_cxl_test
rc=1

destroy_regions()
{
	if [[ "$*" ]]; then
		$CXL destroy-region -f -b cxl_test "$@"
	else
		$CXL destroy-region -f -b cxl_test all
	fi
}

# List x1 root decoders for a memdev matching the jq predicate.
find_x1_decoders()
{
	$CXL list -b cxl_test -D -d root -m "$1" |
		  jq -r ".[] |
		  select($2) |
		  select(.nr_targets == 1) |
		  .decoder"
}

# Pick the first pmem-capable x1 root decoder for a memdev.
find_x1_decoder()
{
	find_x1_decoders "$1" ".pmem_capable == true" | head -n1
}

create_x1_region()
{
	mem="$1"

	decoder=$(find_x1_decoder "$mem")
	if [[ ! $decoder ]]; then
		echo "no suitable decoder found for $mem, skipping"
		return
	fi

	# create region
	region=$($CXL create-region -d "$decoder" -m "$mem" | jq -r ".region")

	if [[ ! $region ]]; then
		echo "create-region failed for $decoder / $mem"
		err "$LINENO"
	fi

	# cycle disable/enable
	$CXL disable-region --bus=cxl_test "$region"
	$CXL enable-region --bus=cxl_test "$region"

	# cycle destroying and creating the same region
	destroy_regions "$region"
	region=$($CXL create-region -d "$decoder" -m "$mem" | jq -r ".region")

	if [[ ! $region ]]; then
		echo "create-region failed for $decoder / $mem"
		err "$LINENO"
	fi
	destroy_regions "$region"
}

create_subregions()
{
	slice=$((256 << 20))
	mem="$1"

	decoder=$(find_x1_decoder "$mem")
	if [[ ! $decoder ]]; then
		echo "no suitable decoder found for $mem, skipping"
		return
	fi

	size="$($CXL list -m "$mem" | jq -r '.[].pmem_size')"
	if [[ ! $size ]]; then
		echo "$mem: unable to determine size"
		err "$LINENO"
	fi

	num_regions=$((size / slice))

	declare -a regions
	for (( i = 0; i < num_regions; i++ )); do
		regions[$i]=$($CXL create-region -d "$decoder" -m "$mem" -s "$slice" | jq -r ".region")
		if [[ ! ${regions[$i]} ]]; then
			echo "create sub-region failed for $decoder / $mem"
			err "$LINENO"
		fi
	done

	echo "created $num_regions subregions:"
	for (( i = 0; i < num_regions; i++ )); do
		echo "${regions[$i]}"
	done

	for (( i = (num_regions - 1); i >= 0; i-- )); do
		destroy_regions "${regions[$i]}"
	done
}

create_single()
{
	# the 5th cxl_test decoder is expected to target a single-port
	# host-bridge. Older cxl_test implementations may not define it,
	# so skip the test in that case.
	decoder=$($CXL list -b cxl_test -D -d root |
		  jq -r ".[4] |
		  select(.pmem_capable == true) |
		  select(.nr_targets == 1) |
		  .decoder")

        if [[ ! $decoder ]]; then
                echo "no single-port host-bridge decoder found, skipping"
                return
        fi

	region=$($CXL create-region -d "$decoder" | jq -r ".region")
	if [[ ! $region ]]; then
		echo "failed to create single-port host-bridge region"
		err "$LINENO"
	fi

	destroy_regions "$region"
}

# Return memdev @index behind root target @position.
find_memdev()
{
	local decoder="$1" position="$2" index="$3"
	local port_dev

	port_dev=$($CXL list -T -d "$decoder" | jq -r ".[] |
		.targets | .[] | select(.position == $position) | .target")
	$CXL list -M -p "$port_dev" | jq -r ".[$index].memdev"
}

create_and_destroy_region()
{
	region=$($CXL create-region -d "$decoder" -m "$memdevs" |
		jq -r ".region")

	if [[ ! $region ]]; then
		echo "create-region failed for $decoder"
		err "$LINENO"
	fi

	destroy_regions "$region"
}

setup_x1()
{
	# Find an x1 decoder
	decoder=$($CXL list -b cxl_test -D -d root | jq -r ".[] |
		select(.pmem_capable == true) |
		select(.nr_targets == 1) |
		.decoder")

	mem0=$(find_memdev "$decoder" 0 0)
	memdevs="$mem0"
}

setup_x2()
{
	# Find an x2 decoder
	decoder=$($CXL list -b cxl_test -D -d root | jq -r ".[] |
		select(.pmem_capable == true) |
		select(.nr_targets == 2) |
		.decoder")

	mem0=$(find_memdev "$decoder" 0 0)
	mem1=$(find_memdev "$decoder" 1 0)
	memdevs="$mem0 $mem1"
}

setup_x4()
{
	# find an x2 decoder
	decoder=$($CXL list -b cxl_test -D -d root | jq -r ".[] |
		select(.pmem_capable == true) |
		select(.nr_targets == 2) |
		.decoder")

	mem0=$(find_memdev "$decoder" 0 0)
	mem1=$(find_memdev "$decoder" 1 0)
	mem2=$(find_memdev "$decoder" 0 1)
	mem3=$(find_memdev "$decoder" 1 1)
	memdevs="$mem0 $mem1 $mem2 $mem3"
}

setup_x3()
{
	# find an x3 decoder
	decoder=$($CXL list -b cxl_test -D -d root | jq -r ".[] |
		select(.pmem_capable == true) |
		select(.nr_targets == 3) |
		.decoder")

	if [[ ! $decoder ]]; then
		echo "no x3 decoder found, skipping xor-x3 test"
		return
	fi

	mem0=$(find_memdev "$decoder" 0 0)
	mem1=$(find_memdev "$decoder" 1 0)
	mem2=$(find_memdev "$decoder" 2 0)
	memdevs="$mem0 $mem1 $mem2"
}

# test region creation on devices behind a single-port host-bridge
create_single

for mem in ${mems[@]}; do
	create_x1_region "$mem"
done

# test multiple subregions under the same decoder, using slices of the same memdev
# to test out back-to-back pmem DPA allocations on memdevs
for mem in ${mems[@]}; do
	create_subregions "$mem"
done

# Repeat x1/x2/x3/x4 region creation with XOR interleave arithmetic.
load_cxl_test interleave_arithmetic=1

setup_x1
create_and_destroy_region
setup_x2
create_and_destroy_region
setup_x4
create_and_destroy_region
# x3 decoder may not be available in cxl/test topo yet
setup_x3
if [[ $decoder ]]; then
	create_and_destroy_region
fi

check_dmesg "$LINENO"

modprobe -r cxl_test
