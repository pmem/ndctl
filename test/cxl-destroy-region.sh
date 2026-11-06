#!/bin/bash
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2023 Intel Corporation. All rights reserved.

. $(dirname $0)/common

rc=77

set -ex

trap 'err $LINENO' ERR

check_prereq "jq"

modprobe -r cxl_test
modprobe cxl_test
rc=1

assert_no_regions()
{
	regions_json="$("$CXL" list -b "$CXL_TEST_BUS" -Ri)"
	[[ -n "$regions_json" ]] || err "$LINENO"
	[[ "$(jq 'length' <<<"$regions_json")" -eq 0 ]] || err "$LINENO"
}

destroy_regions()
{
	if [[ "$*" ]]; then
		"$CXL" destroy-region -f -b "$CXL_TEST_BUS" "$@"
	else
		"$CXL" destroy-region -f -b "$CXL_TEST_BUS" all
	fi
}

create_region()
{
	local decoder="$1"
	local memdev="$2"
	local size="$3"
	local region

	if [[ -n "$size" ]]; then
		region=$("$CXL" create-region -d "$decoder" -m "$memdev" -s "$size" |
			 jq -r ".region")
	else
		region=$("$CXL" create-region -d "$decoder" -m "$memdev" |
			 jq -r ".region")
	fi

	if [[ -z "$region" || "$region" == "null" ]]; then
		echo "create-region failed for decoder=$decoder memdev=$memdev"
		err "$LINENO"
	fi

	echo "$region"
}

check_destroy_ram()
{
	mem=$1
	decoder=$2

	region="$("$CXL" create-region -t ram -d "$decoder" -m "$mem" | jq -r ".region")"
	if [[ ! $region ]]; then
		err "$LINENO"
	fi
	"$CXL" enable-region "$region"

	# default is memory is system-ram offline
	"$CXL" disable-region "$region"
	"$CXL" destroy-region "$region"
}

check_destroy_devdax()
{
	mem=$1
	decoder=$2

	region="$("$CXL" create-region -t ram -d "$decoder" -m "$mem" | jq -r ".region")"
	if [[ ! $region ]]; then
		err "$LINENO"
	fi
	"$CXL" enable-region "$region"

	dax="$("$CXL" list -X -r "$region" | jq -r ".[].daxregion.devices" | jq -r '.[].chardev')"

	$DAXCTL reconfigure-device -m devdax "$dax"

	"$CXL" disable-region "$region"
	"$CXL" destroy-region "$region"
}

find_pmem_decoder()
{
	local mem="$1"
	local slice="$2"
	local decoder

	decoder=$($CXL list -b cxl_test -D -d root -m "$mem" |
		jq -r ".[] |
		select(.pmem_capable == true) |
		select(.nr_targets == 1) |
		select(.max_available_extent >= $(( slice * 2 ))) |
		.decoder" | head -n1)

	[[ -z $decoder || $decoder == "null" ]] && return 1
	echo "$decoder"
}

check_destroy_subregion_order()
{
	local mem="$1"
	local slice=$((256 << 20))
	local decoder
	local region0=""
	local region1=""

	decoder=$(find_pmem_decoder "$mem" "$slice") || return 1

	region0=$(create_region "$decoder" "$mem" "$slice")
	region1=$(create_region "$decoder" "$mem" "$slice")

	# wrong destroy order should fail
	destroy_regions "$region0" && err "$LINENO"

	# region0 should still be enabled
	"$CXL" list -r "$region0" | jq -e 'length > 0' > /dev/null || err "$LINENO"

	# regions should tear down cleanly in correct order
	destroy_regions "$region1" || err "$LINENO"
	"$CXL" list -r "$region1" | jq -e 'length == 0' > /dev/null || err "$LINENO"
	destroy_regions "$region0" || err "$LINENO"
	"$CXL" list -r "$region0" | jq -e 'length == 0' > /dev/null || err "$LINENO"

	return 0
}

# Get clean slate, including auto region resources
destroy_regions
assert_no_regions

# Find a memory device to create regions on to test the destroy
readarray -t mems < <("$CXL" list -b "$CXL_TEST_BUS" -M | jq -r '.[].memdev')
[[ ${#mems[@]} -eq 0 ]] && err "$LINENO"

found=0
for mem in "${mems[@]}"; do
        ramsize="$("$CXL" list -m "$mem" | jq -r '.[].ram_size')"
        if [[ $ramsize == "null" || ! $ramsize ]]; then
                continue
        fi
        decoder="$("$CXL" list -b "$CXL_TEST_BUS" -D -d root -m "$mem" |
                  jq -r ".[] |
                  select(.volatile_capable == true) |
                  select(.nr_targets == 1) |
                  select(.max_available_extent >= ${ramsize}) |
                  .decoder" | head -n1)"
	[[ -z $decoder || $decoder == "null" ]] && continue

	check_destroy_ram "$mem" "$decoder"
	check_destroy_devdax "$mem" "$decoder"
	found=1
	break
done
[[ $found -eq 1 ]] || err "$LINENO"

# test wrong-order destroy on back-to-back pmem regions
destroy_regions
found=0
for mem in "${mems[@]}"; do
	if check_destroy_subregion_order "$mem"; then
		found=1
		break
	fi
done
[[ $found -eq 1 ]] || err "$LINENO"

check_dmesg "$LINENO"

modprobe -r cxl_test
