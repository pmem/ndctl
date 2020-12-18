#!/bin/bash -Ex
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2020, Oracle Corporation.

rc=77
. $(dirname $0)/common

trap 'cleanup $LINENO' ERR

cleanup()
{
	printf "Error at line %d\n" "$1"
	[[ $testdev ]] && reset
	exit $rc
}

find_testdev()
{
	local rc=77

	# The hmem driver is needed to change the device mode, only
	# kernels >= v5.6 might have it available. Skip if not.
	if ! modinfo dax_hmem; then
		# check if dax_hmem is builtin
		if [ ! -d "/sys/module/device_hmem" ]; then
			printf "Unable to find hmem module\n"
			exit $rc
		fi
	fi

	# find a victim region provided by dax_hmem
	testpath=$("$DAXCTL" list -r 0 | jq -er '.[0].path | .//""')
	if [[ ! "$testpath" == *"hmem"* ]]; then
		printf "Unable to find a victim region\n"
		exit "$rc"
	fi

	# find a victim device
	testdev=$("$DAXCTL" list -D -r 0 | jq -er '.[0].chardev | .//""')
	if [[ ! $testdev  ]]; then
		printf "Unable to find a victim device\n"
		exit "$rc"
	fi
	printf "Found victim dev: %s on region id 0\n" "$testdev"
}

setup_dev()
{
	local rc=1
	local nmaps=0
	test -n "$testdev"

	nmaps=$(daxctl_get_nr_mappings "$testdev")
	if [[ $nmaps == 0 ]]; then
		printf "Device is idle"
		exit "$rc"
	fi

	"$DAXCTL" disable-device "$testdev"
	"$DAXCTL" reconfigure-device -s 0 "$testdev"
	available=$("$DAXCTL" list -r 0 | jq -er '.[0].available_size | .//""')
}

reset_dev()
{
	test -n "$testdev"

	"$DAXCTL" disable-device "$testdev"
	"$DAXCTL" reconfigure-device -s "$available" "$testdev"
	"$DAXCTL" enable-device "$testdev"
}

reset()
{
	test -n "$testdev"

	"$DAXCTL" disable-device -r 0 all
	"$DAXCTL" destroy-device -r 0 all
	"$DAXCTL" reconfigure-device -s "$available" "$testdev"
}

clear_dev()
{
	"$DAXCTL" disable-device "$testdev"
	"$DAXCTL" reconfigure-device -s 0 "$testdev"
}

test_pass()
{
	local rc=1

	# Available size
	_available_size=$("$DAXCTL" list -r 0 | jq -er '.[0].available_size | .//""')
	if [[ ! $_available_size == "$available" ]]; then
		echo "Unexpected available size $_available_size != $available"
		exit "$rc"
	fi
}

fail_if_available()
{
	local rc=1

	_size=$("$DAXCTL" list -r 0 | jq -er '.[0].available_size | .//""')
	if [[ $_size ]]; then
		echo "Unexpected available size $_size"
		exit "$rc"
	fi
}

daxctl_get_dev()
{
	"$DAXCTL" list -d "$1" | jq -er '.[].chardev'
}

daxctl_get_mode()
{
	"$DAXCTL" list -d "$1" | jq -er '.[].mode'
}

daxctl_get_size_by_mapping()
{
	local size=0
	local _start=0
	local _end=0

	_start=$(cat "$1"/start)
	_end=$(cat "$1"/end)
	((size=size + _end - _start + 1))
	echo $size
}

daxctl_get_nr_mappings()
{
	local i=0
	local _size=0
	local devsize=0
	local path=""

	path=$(readlink -f /sys/bus/dax/devices/"$1"/)
	until ! [ -d "$path/mapping$i" ]
	do
		_size=$(daxctl_get_size_by_mapping "$path/mapping$i")
		if [[ $_size == 0 ]]; then
			i=0
			break
		fi
		((devsize=devsize + _size))
		((i=i + 1))
	done

	# Return number of mappings if the sizes between size field
	# and the one computed by mappingNNN are the same
	_size=$("$DAXCTL" list -d "$1" | jq -er '.[0].size | .//""')
	if [[ ! $_size == "$devsize" ]]; then
		echo 0
	else
		echo $i
	fi
}

daxctl_test_multi()
{
	local daxdev

	size=$((available / 4))

	if [[ $2 ]]; then
		"$DAXCTL" disable-device "$testdev"
		"$DAXCTL" reconfigure-device -s $size "$testdev"
	fi

	daxdev_1=$("$DAXCTL" create-device -r 0 -s $size | jq -er '.[].chardev')
	test -n "$daxdev_1"

	daxdev_2=$("$DAXCTL" create-device -r 0 -s $size | jq -er '.[].chardev')
	test -n "$daxdev_2"

	if [[ ! $2 ]]; then
		daxdev_3=$("$DAXCTL" create-device -r 0 -s $size | jq -er '.[].chardev')
		test -n "$daxdev_3"
	fi

	# Hole
	"$DAXCTL" disable-device  "$1" && "$DAXCTL" destroy-device "$1"

	# Pick space in the created hole and at the end
	new_size=$((size * 2))
	daxdev_4=$("$DAXCTL" create-device -r 0 -s "$new_size" | jq -er '.[].chardev')
	test -n "$daxdev_4"
	test "$(daxctl_get_nr_mappings "$daxdev_4")" -eq 2

	fail_if_available

	"$DAXCTL" disable-device -r 0 all
	"$DAXCTL" destroy-device -r 0 all
}

daxctl_test_multi_reconfig()
{
	local ncfgs=$1
	local dump=$2
	local daxdev

	size=$((available / ncfgs))

	test -n "$testdev"

	"$DAXCTL" disable-device "$testdev"
	"$DAXCTL" reconfigure-device -s $size "$testdev"
	"$DAXCTL" disable-device "$testdev"

	daxdev_1=$("$DAXCTL" create-device -r 0 -s $size | jq -er '.[].chardev')
	"$DAXCTL" disable-device "$daxdev_1"

	start=$((size + size))
	max=$((size * ncfgs / 2))
	for i in $(seq $start $size $max)
	do
		"$DAXCTL" disable-device "$testdev"
		"$DAXCTL" reconfigure-device -s "$i" "$testdev"

		"$DAXCTL" disable-device "$daxdev_1"
		"$DAXCTL" reconfigure-device -s "$i" "$daxdev_1"
	done

	test "$(daxctl_get_nr_mappings "$testdev")" -eq $((ncfgs / 2))
	test "$(daxctl_get_nr_mappings "$daxdev_1")" -eq $((ncfgs / 2))

	if [[ $dump ]]; then
		"$DAXCTL" list -M -d "$daxdev_1" | jq -er '.[]' > "$dump"
	fi

	fail_if_available

	"$DAXCTL" disable-device "$daxdev_1" && "$DAXCTL" destroy-device "$daxdev_1"
}

daxctl_test_adjust()
{
	local rc=1
	local ncfgs=4
	local daxdev

	size=$((available / ncfgs))

	test -n "$testdev"

	start=$((size + size))
	for i in $(seq 1 1 $ncfgs)
	do
		daxdev=$("$DAXCTL" create-device -r 0 -s "$size" | jq -er '.[].chardev')
		test "$(daxctl_get_nr_mappings "$daxdev")" -eq 1
	done

	daxdev=$(daxctl_get_dev "dax0.1")
	"$DAXCTL" disable-device "$daxdev" && "$DAXCTL" destroy-device "$daxdev"
	daxdev=$(daxctl_get_dev "dax0.4")
	"$DAXCTL" disable-device "$daxdev" && "$DAXCTL" destroy-device "$daxdev"

	daxdev=$(daxctl_get_dev "dax0.2")
	"$DAXCTL" disable-device "$daxdev"
	"$DAXCTL" reconfigure-device -s $((size * 2)) "$daxdev"
	# Allocates space at the beginning: expect two mappings as
	# as don't adjust the mappingX region. This is because we
	# preserve the relative page_offset of existing allocations
	test "$(daxctl_get_nr_mappings "$daxdev")" -eq 2

	daxdev=$(daxctl_get_dev "dax0.3")
	"$DAXCTL" disable-device "$daxdev"
	"$DAXCTL" reconfigure-device -s $((size * 2)) "$daxdev"
	# Adjusts space at the end, expect one mapping because we are
	# able to extend existing region range.
	test "$(daxctl_get_nr_mappings "$daxdev")" -eq 1

	fail_if_available

	daxdev=$(daxctl_get_dev "dax0.3")
	"$DAXCTL" disable-device "$daxdev" && "$DAXCTL" destroy-device "$daxdev"
	daxdev=$(daxctl_get_dev "dax0.2")
	"$DAXCTL" disable-device "$daxdev" && "$DAXCTL" destroy-device "$daxdev"
}

# Test 0:
# Sucessfully zero out the region device and allocate the whole space again.
daxctl_test0()
{
	clear_dev
	test_pass
}

# Test 1:
# Sucessfully creates and destroys a device with the whole available space
daxctl_test1()
{
	local daxdev

	daxdev=$("$DAXCTL" create-device -r 0 | jq -er '.[].chardev')

	test -n "$daxdev"
	test "$(daxctl_get_nr_mappings "$daxdev")" -eq 1
	fail_if_available

	"$DAXCTL" disable-device "$daxdev" && "$DAXCTL" destroy-device "$daxdev"

	clear_dev
	test_pass
}

# Test 2: space at the middle and at the end
# Successfully pick space in the middle and space at the end, by
# having the region device reconfigured with some of the memory.
daxctl_test2()
{
	daxctl_test_multi "dax0.1" 1
	clear_dev
	test_pass
}

# Test 3: space at the beginning and at the end
# Successfully pick space in the beginning and space at the end, by
# having the region device emptied (so region beginning starts with dax0.1).
daxctl_test3()
{
	daxctl_test_multi "dax0.1"
	clear_dev
	test_pass
}

# Test 4: space at the end
# Successfully reconfigure two devices in increasingly bigger allocations.
# The difference is that it reuses an existing resource, and only needs to
# pick at the end of the region
daxctl_test4()
{
	daxctl_test_multi_reconfig 8 ""
	clear_dev
	test_pass
}

# Test 5: space adjust
# Successfully adjusts two resources to fill the whole region
# First adjusts towards the beginning of region, the second towards the end.
daxctl_test5()
{
	daxctl_test_adjust
	clear_dev
	test_pass
}

# Test 6: align
# Successfully creates a device with a align property
daxctl_test6()
{
	local daxdev
	local align
	local size

	# Available size
	size=$available

	# Use 2M by default or 1G if supported
	align=2097152
	if [[ $((available >= 1073741824 )) ]]; then
		align=1073741824
		size=$align
	fi

	daxdev=$("$DAXCTL" create-device -r 0 -s $size -a $align | jq -er '.[].chardev')

	test -n "$daxdev"

	"$DAXCTL" disable-device "$daxdev" && "$DAXCTL" destroy-device "$daxdev"

	clear_dev
	test_pass
}

# Test 7: input device
# Successfully creates a device with an input file from the multi-range
# device test, and checking that we have the same number of mappings/size.
daxctl_test7()
{
	daxctl_test_multi_reconfig 8 "input.json"

	# The parameter should parse the region_id from the chardev entry
	# therefore using the same region_id as test4
	daxdev_1=$("$DAXCTL" create-device --input input.json | jq -er '.[].chardev')

	# Validate if it's the same mappings as done by test4
	# It also validates the size computed from the mappings
	# A zero value means it failed, and four mappings is what's
	# created by daxctl_test4
	test "$(daxctl_get_nr_mappings "$daxdev_1")" -eq 4

	"$DAXCTL" disable-device "$daxdev_1" && "$DAXCTL" destroy-device "$daxdev_1"

	clear_dev
	test_pass
}

find_testdev
rc=1
setup_dev
daxctl_test0
daxctl_test1
daxctl_test2
daxctl_test3
daxctl_test4
daxctl_test5
daxctl_test6
daxctl_test7
reset_dev
exit 0
