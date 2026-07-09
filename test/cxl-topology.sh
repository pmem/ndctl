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

# THEORY OF OPERATION: Validate the hard coded assumptions of the
# cxl_test.ko module that defines its topology in
# tools/testing/cxl/test/cxl.c. If that model ever changes then the
# paired update must be made to this test.

# validate the autodiscovered region
region_json=$("$CXL" list -R -u)
[ -n "$region_json" ] || err "$LINENO"
region=$(jq -r '.region // empty' <<<"$region_json")
region_size=$(jq -r '.size // empty' <<<"$region_json")
region_resource=$(jq -r '.resource // empty' <<<"$region_json")
[ -n "$region" ] || err "$LINENO"
[ -n "$region_size" ] || err "$LINENO"
[ -n "$region_resource" ] || err "$LINENO"

# validate the dax device created for the autodiscovered region
dax_json=$("$DAXCTL" list -r "$region" -DMu)
[ -n "$dax_json" ] || err "$LINENO"
dax_dev=$(jq -r '.chardev // empty' <<<"$dax_json")
dax_size=$(jq -r '.size // empty' <<<"$dax_json")
dax_start=$(jq -r '.mappings[0].start // empty' <<<"$dax_json")
[ -n "$dax_dev" ] || err "$LINENO"
[ "$dax_size" = "$region_size" ] || err "$LINENO"
[ "$dax_start" = "$region_resource" ] || err "$LINENO"

# collect cxl_test root device id
json=$($CXL list -b cxl_test)
count=$(jq "length" <<< $json)
((count == 1)) || err "$LINENO"
root=$(jq -r ".[] | .bus" <<< $json)


# validate 2 or 3 host bridges under a root port
json=$($CXL list -b cxl_test -BP)
count=$(jq ".[] | .[\"ports:$root\"] | length" <<< $json)
((count == 2)) || ((count == 3)) || err "$LINENO"
bridges=$count

bridge_filter()
{
	local br_num="$1"

	jq -r \
		--arg key "$root" \
		--argjson br_num "$br_num" \
		'.[] |
		  select(has("ports:" + $key)) |
		  .["ports:" + $key] |
		  map(
		    {
		      full: .,
		      length: (.["ports:" + .port] | length)
		    }
		  ) |
		  sort_by(-.length) |
		  map(.full) |
		  .[$br_num].port'
}

# $count has already been sanitized for acceptable values, so
# just collect $count bridges here.
for i in $(seq 0 $((count - 1))); do
	bridge[$i]="$(bridge_filter "$i" <<< "$json")"
done

# validate root ports per host bridge
check_host_bridge()
{
	json=$($CXL list -b cxl_test -T -p $1)
	count=$(jq ".[] | .dports | length" <<< $json)
	((count == $2)) || err "$3"
}

check_host_bridge ${bridge[0]} 2 $LINENO
check_host_bridge ${bridge[1]} 2 $LINENO
((bridges > 2)) && check_host_bridge ${bridge[2]} 1 $LINENO

# validate 2 switches per root-port
json=$($CXL list -b cxl_test -P -p ${bridge[0]})
count=$(jq ".[] | .[\"ports:${bridge[0]}\"] | length" <<< $json)
((count == 2)) || err "$LINENO"

port_sort="sort_by(.port | .[4:] | tonumber)"
switch[0]=$(jq -r ".[] | .[\"ports:${bridge[0]}\"] | $port_sort | .[0].host" <<< $json)
switch[1]=$(jq -r ".[] | .[\"ports:${bridge[0]}\"] | $port_sort | .[1].host" <<< $json)

json=$($CXL list -b cxl_test -P -p ${bridge[1]})
count=$(jq ".[] | .[\"ports:${bridge[1]}\"] | length" <<< $json)
((count == 2)) || err "$LINENO"

switch[2]=$(jq -r ".[] | .[\"ports:${bridge[1]}\"] | $port_sort | .[0].host" <<< $json)
switch[3]=$(jq -r ".[] | .[\"ports:${bridge[1]}\"] | $port_sort | .[1].host" <<< $json)


# validate the expected properties of the 4 or 5 root decoders
# use the size of the first decoder to determine the
# cxl_test version / properties
json=$($CXL list -b cxl_test -D -d root)
port_id=${root:4}
port_id_len=${#port_id}
decoder_sort="sort_by(.decoder | .[$((8+port_id_len)):] | tonumber)"
count=$(jq "[ $decoder_sort | .[0] |
	select(.volatile_capable == true) |
	select(.size == $((256 << 20))) |
	select(.nr_targets == 1) ] | length" <<< $json)

if [ $count -eq 1 ]; then
	decoder_base_size=$((256 << 20))
	pmem_size=$((256 << 20))
else
	decoder_base_size=$((1 << 30))
	pmem_size=$((1 << 30))
fi

count=$(jq "[ $decoder_sort | .[1] |
	select(.volatile_capable == true) |
	select(.size == $((decoder_base_size * 2))) |
	select(.nr_targets == 2) ] | length" <<< $json)
((count == 1)) || err "$LINENO"

count=$(jq "[ $decoder_sort | .[2] |
	select(.pmem_capable == true) |
	select(.size == $decoder_base_size) |
	select(.nr_targets == 1) ] | length" <<< $json)
((count == 1)) || err "$LINENO"

count=$(jq "[ $decoder_sort | .[3] |
	select(.pmem_capable == true) |
	select(.size == $((decoder_base_size * 2))) |
	select(.nr_targets == 2) ] | length" <<< $json)
((count == 1)) || err "$LINENO"

if (( bridges == 3 )); then
	count=$(jq "[ $decoder_sort | .[4] |
		select(.pmem_capable == true) |
		select(.size == $decoder_base_size) |
		select(.nr_targets == 1) ] | length" <<< $json)
	((count == 1)) || err "$LINENO"
fi

# check that all 8 or 10 cxl_test memdevs are enabled by default and have a
# pmem size of 256M, or 1G
json=$($CXL list -b cxl_test -M)
count=$(jq "map(select(.pmem_size == $pmem_size)) | length" <<< $json)
((bridges == 2 && count == 8 || bridges == 3 && count == 10 ||
  bridges == 4 && count == 11)) || err "$LINENO"

# Test that switch port decoders have complete target list enumeration
# Validates a fix for multiple decoders sharing the same dport.
# Based on the cxl_test topology expectation of switch ports at depth 2
# with 8 decoders each. Adjust if that expectation changes.
test_switch_decoder_target_enumeration() {

	# Get verbose output to see targets arrays
	json=$($CXL list -b cxl_test -vvv)

	switch_port_issues=$(jq '
	# Find all switch ports (depth 2)
	[.. | objects | select(.depth == 2 and has("decoders:" + .port))] |

	# For each switch port, analyze its decoder target pattern
	map({
		port: .port,
		nr_dports: .nr_dports,

		# Count non-endpoint decoders (no "mode" field)
		total: ([to_entries[] | select(.key | startswith("decoders:"))
			| .value[] | select(has("mode") == false)] |
			length),

		# Count how many have targets
		with_targets: ([to_entries[] | select(.key |
			startswith("decoders:")) | .value[] |
			select(has("mode") == false and .nr_targets > 0)] |
			length),

		# Count how many explicitly have no targets
		without_targets: ([to_entries[] | select(.key |
			startswith("decoders:")) | .value[] |
			select(has("mode") == false and .nr_targets == 0)] |
			length)
		}) |

		# Filter for the expected pattern and count them
		map(select(.nr_dports > 0 and
			   .with_targets == 1 and
			   .without_targets >= 7)) |
			   length
	' <<<"$json")

	((switch_port_issues == 0)) || {
		echo "Found $switch_port_issues switch ports with incomplete target enumeration"
		echo "Only 1 decoder has targets while 7+ have nr_targets=0"
		err "$LINENO"
	}
}
# Skip the target enumeration test where known broken
check_eq_kver 6.18 || test_switch_decoder_target_enumeration

# check that switch ports disappear after all of their memdevs have been
# disabled, and return when the memdevs are enabled.
for s in ${switch[@]}
do
	json=$($CXL list -M -p $s)
	count=$(jq "length" <<< $json)
	((count == 2)) || err "$LINENO"

	mem[0]=$(jq -r ".[0] | .memdev" <<< $json)
	mem[1]=$(jq -r ".[1] | .memdev" <<< $json)

	$CXL disable-memdev ${mem[0]} --force
	json=$($CXL list -p $s)
	count=$(jq "length" <<< $json)
	((count == 1)) || err "$LINENO"

	$CXL disable-memdev ${mem[1]} --force
	json=$($CXL list -p $s)
	count=$(jq "length" <<< $json)
	((count == 0)) || err "$LINENO"

	$CXL enable-memdev ${mem[0]}
	$CXL enable-memdev ${mem[1]}

	json=$($CXL list -p $s)
	count=$(jq "length" <<< $json)
	((count == 1)) || err "$LINENO"

	$CXL disable-port $s --force
	json=$($CXL list -p $s)
	count=$(jq "length" <<< $json)
	((count == 0)) || err "$LINENO"

	$CXL enable-memdev ${mem[0]} ${mem[1]}
	json=$($CXL list -p $s)
	count=$(jq "length" <<< $json)
	((count == 1)) || err "$LINENO"
done


# validate host bridge tear down for the first 2 bridges
for b in ${bridge[0]} ${bridge[1]}
do
	$CXL disable-port $b -f
	json=$($CXL list -M -i -p $b)
	count=$(jq "map(select(.state == \"disabled\")) | length" <<< $json)
	((count == 4)) || err "$LINENO"

	$CXL enable-port $b -m
	json=$($CXL list -M -p $b)
	count=$(jq "length" <<< $json)
	((count == 4)) || err "$LINENO"
done


# validate that the bus can be disabled without issue
$CXL disable-bus $root -f

test_zero_size_decoders() {

	if ! modinfo cxl_test | grep -q '^parm:.*mock_zero_size_decoders'; then
		return 0
	fi

	modprobe -r cxl_test
	modprobe cxl_test mock_zero_size_decoders=1

	# the auto-region maps 2 active endpoint decoders, one per memdev
	region_json=$("$CXL" list -b cxl_test -R -T -u)
	[ -n "$region_json" ] || err "$LINENO"
	mapfile -t endpoint_decoders < <(
		jq -r '.mappings[]?.decoder // empty' <<<"$region_json"
	)
	((${#endpoint_decoders[@]} == 2)) || err "$LINENO"

	# per endpoint, decoder 0 is active and cxl_test hard codes ids 1
	# and 2 as the committed, locked zero-sized decoders. Other ids may
	# also report size 0 while unlocked, so match on locked to pick out
	# the committed pair. If that model ever changes then the paired
	# update must be made here. size and locked come from sysfs as there
	# is no cxl list representation yet.
	for decoder in "${endpoint_decoders[@]}"; do
		[[ "$decoder" == *.0 ]] || err "$LINENO"

		for id in 1 2; do
			empty_decoder="${decoder%.*}.$id"
			decoder_path="/sys/bus/cxl/devices/$empty_decoder"
			[ -d "$decoder_path" ] || err "$LINENO"

			size=$(cat "$decoder_path/size")
			locked=$(cat "$decoder_path/locked")
			((size == 0)) || err "$LINENO"
			((locked == 1)) || err "$LINENO"
		done
	done
}

test_zero_size_decoders

check_dmesg "$LINENO"

modprobe -r cxl_test
