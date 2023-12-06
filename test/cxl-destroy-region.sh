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

check_destroy_ram()
{
	mem=$1
	decoder=$2

	region="$("$CXL" create-region -d "$decoder" -m "$mem" | jq -r ".region")"
	if [ "$region" == "null" ]; then
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

	region="$("$CXL" create-region -d "$decoder" -m "$mem" | jq -r ".region")"
	if [ "$region" == "null" ]; then
		err "$LINENO"
	fi
	"$CXL" enable-region "$region"

	dax="$("$CXL" list -X -r "$region" | jq -r ".[].daxregion.devices" | jq -r '.[].chardev')"

	$DAXCTL reconfigure-device -m devdax "$dax"

	"$CXL" disable-region "$region"
	"$CXL" destroy-region "$region"
}

# Find a memory device to create regions on to test the destroy
readarray -t mems < <("$CXL" list -b "$CXL_TEST_BUS" -M | jq -r '.[].memdev')
for mem in "${mems[@]}"; do
        ramsize="$("$CXL" list -m "$mem" | jq -r '.[].ram_size')"
        if [[ $ramsize == "null" ]]; then
                continue
        fi
        decoder="$("$CXL" list -b "$CXL_TEST_BUS" -D -d root -m "$mem" |
                  jq -r ".[] |
                  select(.volatile_capable == true) |
                  select(.nr_targets == 1) |
                  select(.size >= ${ramsize}) |
                  .decoder")"
        if [[ $decoder ]]; then
		check_destroy_ram "$mem" "$decoder"
		check_destroy_devdax "$mem" "$decoder"
                break
        fi
done

check_dmesg "$LINENO"

modprobe -r cxl_test
