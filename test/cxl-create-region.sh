#!/bin/bash
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2022 Intel Corporation. All rights reserved.

. $(dirname $0)/common

rc=77

set -ex

trap 'err $LINENO' ERR

check_prereq "jq"

modprobe -r cxl_test
modprobe cxl_test
rc=1
udevadm settle

destroy_regions()
{
	if [[ "$*" ]]; then
		$CXL destroy-region -f -b cxl_test "$@"
	else
		$CXL destroy-region -f -b cxl_test all
	fi
}

create_x1_region()
{
	mem="$1"

	# find a pmem capable root decoder for this mem
	decoder=$($CXL list -b cxl_test -D -d root -m "$mem" |
		  jq -r ".[] |
		  select(.pmem_capable == true) |
		  select(.nr_targets == 1) |
		  .decoder")

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

	# find a pmem capable root decoder for this mem
	decoder=$($CXL list -b cxl_test -D -d root -m "$mem" |
		  jq -r ".[] |
		  select(.pmem_capable == true) |
		  select(.nr_targets == 1) |
		  .decoder")

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
		udevadm settle
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

# test region creation on devices behind a single-port host-bridge
create_single

# test reading labels directly through cxl-cli
readarray -t mems < <("$CXL" list -b cxl_test -M | jq -r '.[].memdev')

for mem in ${mems[@]}; do
	create_x1_region "$mem"
done

# test multiple subregions under the same decoder, using slices of the same memdev
# to test out back-to-back pmem DPA allocations on memdevs
for mem in ${mems[@]}; do
	create_subregions "$mem"
done

modprobe -r cxl_test
