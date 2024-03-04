#!/bin/bash
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024 Intel Corporation. All rights reserved.

. $(dirname $0)/common

rc=77

set -ex

trap 'err $LINENO' ERR

check_prereq "jq"

modprobe -r cxl_test
modprobe cxl_test
rc=1

check_qos_decoders () {
	# check root decoders have expected fake qos_class
	# also make sure the number of root decoders equal to the number
	# with qos_class found
	json=$($CXL list -b cxl_test -D -d root)
	num_decoders=$(echo "$json" | jq length)
	count=0
	while read -r qos_class; do
		if [[ "$qos_class" != "$CXL_TEST_QOS_CLASS" ]]; then
			err "$LINENO"
		fi
		count=$((count+1))
	done <<< "$(echo "$json" | jq -r '.[] | .qos_class')"

	if [[ "$count" != "$num_decoders" ]]; then
		err "$LINENO"
	fi
}

check_qos_memdevs () {
	# Check that memdevs that expose ram_qos_class or pmem_qos_class have
	# expected fake value programmed.
	json=$($CXL list -b cxl_test -M)
	num_memdevs=$(echo "$json" | jq length)

	for (( i = 0; i < num_memdevs; i++ )); do
		ram_size="$(jq ".[$i] | .ram_size" <<< "$json")"
		ram_qos_class="$(jq ".[$i] | .ram_qos_class" <<< "$json")"
		pmem_size="$(jq ".[$i] | .pmem_size" <<< "$json")"
		pmem_qos_class="$(jq ".[$i] | .pmem_qos_class" <<< "$json")"

		if [[ "$ram_size" != null ]] && ((ram_qos_class != CXL_TEST_QOS_CLASS)); then
			err "$LINENO"
		fi

		if [[ "$pmem_size" != null ]] && ((pmem_qos_class != CXL_TEST_QOS_CLASS)); then
			err "$LINENO"
		fi
	done
}

# Based on cxl-create-region.sh create_single()
destroy_regions()
{
	if [[ "$*" ]]; then
		$CXL destroy-region -f -b cxl_test "$@"
	else
		$CXL destroy-region -f -b cxl_test all
	fi
}

create_region_check_qos()
{
	# Find an x1 decoder
	decoder=$($CXL list -b cxl_test -D -d root | jq -r "[ .[] |
		  select(.max_available_extent > 0) |
		  select(.pmem_capable == true) |
		  select(.nr_targets == 1) ] |
		  .[0].decoder")

	# Find a memdev for this host-bridge
	port_dev0="$("$CXL" list -T -d "$decoder" | jq -r ".[] |
		    .targets | .[] | select(.position == 0) | .target")"
	mem0="$("$CXL" list -M -p "$port_dev0" | jq -r ".[0].memdev")"
	memdevs="$mem0"

	# Send create-region with -Q to enforce qos_class matching
	region="$("$CXL" create-region -Q -d "$decoder" -m "$memdevs" | jq -r ".region")"
	if [[ ! $region ]]; then
		echo "failed to create region"
		err "$LINENO"
	fi

	destroy_regions "$region"
}

check_qos_decoders
check_qos_memdevs
create_region_check_qos
check_dmesg "$LINEO"

modprobe -r cxl_test
