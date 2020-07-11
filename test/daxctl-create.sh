#!/bin/bash -Ex
# SPDX-License-Identifier: GPL-2.0
# Copyright(c) 2020, Oracle and/or its affiliates.

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
	if ! modinfo hmem; then
		# check if hmem is builtin
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
	test -n "$testdev"

	"$DAXCTL" disable-device "$testdev"
	"$DAXCTL" reconfigure-device -s 0 "$testdev"
	available=$("$DAXCTL" list -r 0 | jq -er '.[0].available_size | .//""')
}

reset_dev()
{
	test -n "$testdev"

	"$DAXCTL" disable-device "$testdev"
	"$DAXCTL" reconfigure-device -s $available "$testdev"
}

reset()
{
	test -n "$testdev"

	"$DAXCTL" disable-device all
	"$DAXCTL" destroy-device all
	"$DAXCTL" reconfigure-device -s $available "$testdev"
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
	if [[ ! $_available_size == $available ]]; then
		printf "Unexpected available size $_available_size != $available\n"
		exit "$rc"
	fi
}

fail_if_available()
{
	local rc=1

	_size=$("$DAXCTL" list -r 0 | jq -er '.[0].available_size | .//""')
	if [[ $_size ]]; then
		printf "Unexpected available size $_size\n"
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

daxctl_test_multi()
{
	local daxdev

	size=$(expr $available / 4)

	if [[ $2 ]]; then
		"$DAXCTL" disable-device "$testdev"
		"$DAXCTL" reconfigure-device -s $size "$testdev"
	fi

	daxdev_1=$("$DAXCTL" create-device -s $size | jq -er '.[].chardev')
	test -n $daxdev_1

	daxdev_2=$("$DAXCTL" create-device -s $size | jq -er '.[].chardev')
	test -n $daxdev_2

	if [[ ! $2 ]]; then
		daxdev_3=$("$DAXCTL" create-device -s $size | jq -er '.[].chardev')
		test -n $daxdev_3
	fi

	# Hole
	"$DAXCTL" disable-device  $1 &&	"$DAXCTL" destroy-device  $1

	# Pick space in the created hole and at the end
	new_size=$(expr $size \* 2)
	daxdev_4=$("$DAXCTL" create-device -s $new_size | jq -er '.[].chardev')
	test -n $daxdev_4

	fail_if_available

	"$DAXCTL" disable-device all
	"$DAXCTL" destroy-device all
}

daxctl_test_multi_reconfig()
{
	local ncfgs=$1
	local daxdev

	size=$(expr $available / $ncfgs)

	test -n "$testdev"

	"$DAXCTL" disable-device "$testdev"
	"$DAXCTL" reconfigure-device -s $size "$testdev"
	"$DAXCTL" disable-device "$testdev"

	daxdev_1=$("$DAXCTL" create-device -s $size | jq -er '.[].chardev')
	"$DAXCTL" disable-device "$daxdev_1"

	start=$(expr $size + $size)
	max=$(expr $ncfgs / 2 \* $size)
	for i in $(seq $start $size $max)
	do
		"$DAXCTL" disable-device "$testdev"
		"$DAXCTL" reconfigure-device -s $i "$testdev"

		"$DAXCTL" disable-device "$daxdev_1"
		"$DAXCTL" reconfigure-device -s $i "$daxdev_1"
	done

	fail_if_available

	"$DAXCTL" disable-device "$daxdev_1" && "$DAXCTL" destroy-device "$daxdev_1"
}

daxctl_test_adjust()
{
	local rc=1
	local ncfgs=4
	local daxdev

	size=$(expr $available / $ncfgs)

	test -n "$testdev"

	start=$(expr $size + $size)
	for i in $(seq 1 1 $ncfgs)
	do
		daxdev=$("$DAXCTL" create-device -s $size)
	done

	daxdev=$(daxctl_get_dev "dax0.1")
	"$DAXCTL" disable-device "$daxdev" && "$DAXCTL" destroy-device "$daxdev"
	daxdev=$(daxctl_get_dev "dax0.4")
	"$DAXCTL" disable-device "$daxdev" && "$DAXCTL" destroy-device "$daxdev"

	daxdev=$(daxctl_get_dev "dax0.2")
	"$DAXCTL" disable-device "$daxdev"
	"$DAXCTL" reconfigure-device -s $(expr $size \* 2) "$daxdev"

	daxdev=$(daxctl_get_dev "dax0.3")
	"$DAXCTL" disable-device "$daxdev"
	"$DAXCTL" reconfigure-device -s $(expr $size \* 2) "$daxdev"

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

	daxdev=$("$DAXCTL" create-device | jq -er '.[].chardev')

	test -n "$daxdev"
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
	daxctl_test_multi_reconfig 8
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

find_testdev
rc=1
setup_dev
daxctl_test0
daxctl_test1
daxctl_test2
daxctl_test3
daxctl_test4
daxctl_test5
reset_dev
exit 0
