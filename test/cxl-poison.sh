#!/bin/bash
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2023 Intel Corporation. All rights reserved.

. "$(dirname "$0")"/common

rc=77

set -ex
[ -d "/sys/kernel/tracing" ] || do_skip "test requires CONFIG_TRACING"

trap 'err $LINENO' ERR

check_prereq "jq"

# THEORY OF OPERATION: Exercise cxl-cli and cxl driver ability to
# inject, clear, and get the poison list. Do it by memdev and by region.

find_memdev()
{
	readarray -t capable_mems < <("$CXL" list -b "$CXL_TEST_BUS" -M |
		jq -r ".[] | select(.pmem_size != null) |
		select(.ram_size != null) |
		select(.poison_injectable == true) | .memdev")

	if [ ${#capable_mems[@]} == 0 ]; then
		echo "no memdevs found for test"
		err "$LINENO"
	fi

	memdev=${capable_mems[0]}
}

find_auto_region()
{
	region="$($CXL list -b "$CXL_TEST_BUS" -R | jq -r ".[0].region")"
	[[ -n "$region" && "$region" != "null" ]] || do_skip "no test region found"
	mem0="$($CXL list -r "$region" --targets | jq -r ".[0].mappings[0].memdev")"
	[[ -n "$mem0" && "$mem0" != "null" ]] || do_skip "no region target0 found"
	mem1="$($CXL list -r "$region" --targets | jq -r ".[0].mappings[1].memdev")"
	[[ -n "$mem1" && "$mem1" != "null" ]] || do_skip "no region target1 found"
	echo "$region"
}

_do_poison()
{
	local action="$1" dev="$2" addr="$3"
	local expect_fail=${4:-false}

	# Regions use sysfs, memdevs use cxl-cli commands
	if [[ "$dev" =~ ^region ]]; then
		local sysfs_path="/sys/kernel/debug/cxl/$dev/${action}_poison"
		"$expect_fail" && echo "$addr" > "$sysfs_path" && err "$LINENO"
		"$expect_fail" || echo "$addr" > "$sysfs_path"
		return
	fi

	case "$action" in
	inject) local cmd=("$CXL" inject-media-poison "$dev" -a "$addr") ;;
	clear)	local cmd=("$CXL" clear-media-poison "$dev" -a "$addr") ;;
	*)	err "$LINENO" ;;
	esac

	"$expect_fail" && "${cmd[@]}" && err "$LINENO"
	"$expect_fail" || "${cmd[@]}"
}

inject_poison()
{
	_do_poison 'inject' "$@"
}

clear_poison()
{
	_do_poison 'clear' "$@"
}

check_trace_entry()
{
	local expected_region="$1"
	local expected_hpa="$2"		# hex or decimal
	local expected_memdev="$3"	# optional
	local expected_dpa="$4"		# optional, hex or decimal
	local trace_line trace_region trace_memdev trace_hpa trace_dpa

	trace_line=$(tail -n 1 /sys/kernel/tracing/trace | grep "cxl_poison")
	 [[ -n "$trace_line" ]] || err "$LINENO"

	trace_region=$(echo "$trace_line" | grep -o 'region=[^ ]*' | cut -d= -f2)
	trace_memdev=$(echo "$trace_line" | grep -o 'memdev=[^ ]*' | cut -d= -f2)

	# Convert HPA and DPA from hex to decimal
        trace_hpa=$(($(echo "$trace_line" | grep -o 'hpa=0x[0-9a-fA-F]\+' | cut -d= -f2)))
        trace_dpa=$(($(echo "$trace_line" | grep -o 'dpa=0x[0-9a-fA-F]\+' | cut -d= -f2)))

	# Convert expected values to decimal
	expected_hpa=$((expected_hpa))
	[[ -n "$expected_dpa" ]] && expected_dpa=$((expected_dpa))

	# Required checks
	[[ "$trace_region" == "$expected_region" ]] || err "$LINENO"
	[[ "$trace_hpa" == "$expected_hpa" ]] || err "$LINENO"

	# Optional checks only enforced if expected value is provided
	[[ -z "$expected_memdev" || "$trace_memdev" == "$expected_memdev" ]] || err "$LINENO"
	[[ -z "$expected_dpa" || "$trace_dpa" == "$expected_dpa" ]] || err "$LINENO"
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

test_poison_by_memdev_by_dpa()
{
	find_memdev
	inject_poison "$memdev" "0x40000000"
	inject_poison "$memdev" "0x40001000"
	inject_poison "$memdev" "0x600"
	inject_poison "$memdev" "0x0"
	validate_poison_found "-m $memdev" 4

	clear_poison "$memdev" "0x40000000"
	clear_poison "$memdev" "0x40001000"
	clear_poison "$memdev" "0x600"
	clear_poison "$memdev" "0x0"
	validate_poison_found "-m $memdev" 0
}

test_poison_by_region_by_dpa()
{
	inject_poison "$mem0" "0"
	inject_poison "$mem1" "0"
	validate_poison_found "-r $region" 2

	clear_poison "$mem0" "0"
	clear_poison "$mem1" "0"
	validate_poison_found "-r $region" 0
}

test_poison_by_region_offset()
{
	local base gran hpa1 hpa2 cache_size
	base=$(cat /sys/bus/cxl/devices/"$region"/resource)
	gran=$(cat /sys/bus/cxl/devices/"$region"/interleave_granularity)
	cache_size=0

	if [ -f "/sys/bus/cxl/devices/$region/extended_linear_cache_size" ]; then
		cache_size=$(cat /sys/bus/cxl/devices/"$region"/extended_linear_cache_size)
	fi

	if [[ $cache_size -gt 0 ]]; then
		base=$((base + cache_size))
	fi

	# Test two HPA addresses: base and base + granularity
	# This hits the two memdevs in the region interleave.
	hpa1=$(printf "0x%x" $((base)))
	hpa2=$(printf "0x%x" $((base + gran)))

	# Inject at the offset and check result using the hpa
	# ABI takes an offset, but recall the hpa to check trace event

	inject_poison "$region" "$cache_size"
	check_trace_entry "$region" "$hpa1"
	inject_poison "$region" "$((gran + cache_size))"
	check_trace_entry "$region" "$hpa2"
	validate_poison_found "-r $region" 2

	clear_poison "$region" "$cache_size"
	check_trace_entry "$region" "$hpa1"
	clear_poison "$region" "$((gran + cache_size))"
	check_trace_entry "$region" "$hpa2"
	validate_poison_found "-r $region" 0
}

test_poison_by_region_offset_negative()
{
	local region_size cache_size cache_offset exceed_offset large_offset
	region_size=$(cat /sys/bus/cxl/devices/"$region"/size)
	cache_size=0

	# Try to get the ELC size attribute
	if [ -f "/sys/bus/cxl/devices/$region/extended_linear_cache_size" ]; then
		cache_size=$(cat /sys/bus/cxl/devices/"$region"/extended_linear_cache_size)
	fi

	# Offset within extended linear cache (if cache_size > 0)
	if [[ $cache_size -gt 0 ]]; then
		cache_offset=$((cache_size - 1))
		echo "Testing offset within cache: $cache_offset (cache_size: $cache_size)"
		inject_poison "$region" "$cache_offset" true
		clear_poison "$region" "$cache_offset" true
	else
		echo "Skipping cache test - cache_size is 0"
	fi

	# Offset exceeds region size
	exceed_offset=$((region_size))
	inject_poison "$region" "$exceed_offset" true
	clear_poison "$region" "$exceed_offset" true

	# Offset exceeds region size by a lot
	large_offset=$((region_size * 2))
	inject_poison "$region" "$large_offset" true
	clear_poison "$region" "$large_offset" true
}

# Backstop a driver fix where a fully mapped partition prematurely
# terminated the unmapped poison scan.
test_poison_unmapped_later_partition()
{
	local decoder ram_size pmem_dpa region

	check_min_kver "7.4" || return 0

	# Free the auto region to use the ram capacity
	$CXL destroy-region -f -b "$CXL_TEST_BUS" all

	find_memdev

	# Fully map the ram partition so it has a zero-length unmapped tail
	decoder=$($CXL list -b "$CXL_TEST_BUS" -D -d root -m "$memdev" |
		  jq -r ".[] |
		  select(.volatile_capable == true) |
		  select(.nr_targets == 1) |
		  .decoder")
	[[ -n "$decoder" && "$decoder" != "null" ]] ||
		do_skip "no x1 volatile decoder found"

	ram_size=$($CXL list -m "$memdev" | jq -r ".[0].ram_size")
	[[ -n "$ram_size" && "$ram_size" != "null" ]] || err "$LINENO"

	region=$($CXL create-region -t ram -d "$decoder" -m "$memdev" \
		 -s "$ram_size" | jq -r ".region")
	[[ -n "$region" && "$region" != "null" ]] || err "$LINENO"

	# Poison the unmapped pmem tail
	pmem_dpa=$ram_size
	inject_poison "$memdev" "$pmem_dpa"
	validate_poison_found "-m $memdev" 1
	clear_poison "$memdev" "$pmem_dpa"
	validate_poison_found "-m $memdev" 0

	$CXL destroy-region -f -b "$CXL_TEST_BUS" "$region"
}

is_unaligned() {
	local region=$1
	local hbiw=$2
	local align addr
	local unit=$((256 * 1024 * 1024))	# 256MB

	# Unaligned regions resources start at addresses that are
	# not aligned to Host Bridge Interleave Ways * 256MB.

	[[ -n "$region" && -n "$hbiw" ]] || err "$LINENO"
	addr="$($CXL list -r "$region" | jq -r '.[0].resource')"
	[[ -n "$addr" && "$addr" != "null" ]] || err "$LINENO"

	align=$((hbiw * unit))
	((addr % align != 0))
}

create_3way_interleave_region()
{
	# find an x3 decoder
	decoder=$($CXL list -b cxl_test -D -d root | jq -r ".[] |
		select(.pmem_capable == true) |
		select(.nr_targets == 3) |
		.decoder")
	[[ $decoder ]] || err "$LINENO"

	# Find a memdev for each host-bridge interleave position
	port_dev0=$($CXL list -T -d "$decoder" | jq -r ".[] |
		.targets | .[] | select(.position == 0) | .target")
	port_dev1=$($CXL list -T -d "$decoder" | jq -r ".[] |
		.targets | .[] | select(.position == 1) | .target")
	port_dev2=$($CXL list -T -d "$decoder" | jq -r ".[] |
		.targets | .[] | select(.position == 2) | .target")
	mem0=$($CXL list -M -p "$port_dev0" | jq -r ".[0].memdev")
	mem1=$($CXL list -M -p "$port_dev1" | jq -r ".[0].memdev")
	mem2=$($CXL list -M -p "$port_dev2" | jq -r ".[0].memdev")
	memdevs="$mem0 $mem1 $mem2"

	region=$($CXL create-region -d "$decoder" -m "$memdevs" |
		jq -r ".region")
	[[ $region ]] || err "$LINENO"
}

verify_offset_translation()
{
    local region="$1"
    local region_resource="$2"

	# Verify that clearing by region offset maps to the same memdev/DPA
	# as a previous clear by memdev/DPA

	# Extract HPA, DPA, and memdev from the previous clear trace event
	local trace_line memdev hpa dpa
	trace_line=$(tail -n 1 /sys/kernel/tracing/trace | grep "cxl_poison")
	[[ -n "$trace_line" ]] || err "$LINENO"

	memdev=$(echo "$trace_line" | grep -o 'memdev=[^ ]*' | cut -d= -f2)
	# Convert HPA and DPA to decimal
	hpa=$(($(echo "$trace_line" | grep -o 'hpa=0x[0-9a-fA-F]\+' |cut -d= -f2)))
	dpa=$(($(echo "$trace_line" | grep -o 'dpa=0x[0-9a-fA-F]\+' | cut -d= -f2)))
	[[ -n "$memdev" && -n "$hpa" && -n "$dpa" ]] || err "$LINENO"

	# Issue a clear poison using the found region offset
	local region_offset=$((hpa - region_resource))
	clear_poison "$region" "$region_offset"

	# Verify the trace event produces the same memdev/DPA for region HPA
	check_trace_entry "$region" "$hpa" "$memdev" "$dpa"
}

run_unaligned_poison_test()
{
	create_3way_interleave_region
	is_unaligned "$region" 3 ||
		do_skip "unaligned region not available for testing"

	# Get region start address and interleave granularity
	read -r region_resource region_gran <<< "$($CXL list -r "$region" |
		jq -r '.[0] | "\(.resource) \(.interleave_granularity)"')"

	# Loop over the 3 memdevs in the region
	for pos in 0 1 2; do
		# Get memdev and decoder
		memdev=$($CXL list -r "$region" --targets |
			jq -r ".[0].mappings[$pos].memdev")
		decoder=$($CXL list -r "$region" --targets |
			jq -r ".[0].mappings[$pos].decoder")

		# Get decoder DPA start
		base_dpa=$($CXL list -d "$decoder" | jq -r '.[0].dpa_resource')

		# Two samples: base and base + interleave granularity
		for offset in 0 "$region_gran"; do
			clear_poison "$memdev" $((base_dpa + offset))
			verify_offset_translation "$region" "$region_resource"
		done
	done
}

run_poison_test()
{
	# Clear old trace events, enable cxl_poison, enable global tracing
	echo "" > /sys/kernel/tracing/trace
	echo 1 > /sys/kernel/tracing/events/cxl/cxl_poison/enable
	echo 1 > /sys/kernel/tracing/tracing_on

	test_poison_by_memdev_by_dpa
	find_auto_region
	test_poison_by_region_by_dpa
	[ -f "/sys/kernel/debug/cxl/$region/inject_poison" ] ||
		do_skip "test cases requires inject by region kernel support"
	test_poison_by_region_offset
	test_poison_by_region_offset_negative
	test_poison_unmapped_later_partition
}

modprobe -r cxl_test
modprobe cxl_test
rc=1
run_poison_test

# An ELC region first appears in the cxl_test module in 6.19
if check_min_kver "6.19"; then
	modprobe -r cxl_test
	modprobe cxl_test extended_linear_cache=1

	[ -f /sys/module/cxl_test/parameters/extended_linear_cache ] || \
	do_skip "cxl_test extended_linear_cache module param not available"

	rc=1
	run_poison_test
fi

# Unaligned address translation first appears in the CXL driver in 7.0
if check_min_kver "7.0"; then
	modprobe -r cxl_test
	# HBIW of 3 happens to only be available w XOR at the moment
	modprobe cxl_test interleave_arithmetic=1

	rc=1
	run_unaligned_poison_test
fi

check_dmesg "$LINENO"

modprobe -r cxl_test
