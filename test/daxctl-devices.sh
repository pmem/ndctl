#!/bin/bash -Ex
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2019-2020 Intel Corporation. All rights reserved.

rc=77
. $(dirname $0)/common

trap 'cleanup $LINENO' ERR

cleanup()
{
	printf "Error at line %d\n" "$1"
	[[ $testdev ]] && reset_dev
	exit $rc
}

find_testdev()
{
	local rc=77

	# The kmem driver is needed to change the device mode, only
	# kernels >= v5.1 might have it available. Skip if not.
	if ! modinfo kmem; then
		# check if kmem is builtin
		if ! grep -qF "kmem" "/lib/modules/$(uname -r)/modules.builtin"; then
			printf "Unable to find kmem module\n"
			exit $rc
		fi
	fi

	# find a victim device
	testbus="$ACPI_BUS"
	testdev=$("$NDCTL" list -b "$testbus" -Ni | jq -er '.[0].dev | .//""')
	if [[ ! $testdev  ]]; then
		printf "Unable to find a victim device\n"
		exit "$rc"
	fi
	printf "Found victim dev: %s on bus: %s\n" "$testdev" "$testbus"
}

setup_dev()
{
	test -n "$testbus"
	test -n "$testdev"

	"$NDCTL" destroy-namespace -f -b "$testbus" "$testdev"
	testdev=$("$NDCTL" create-namespace -b "$testbus" -m devdax -fe "$testdev" -s 256M | \
		jq -er '.dev')
	test -n "$testdev"
}

reset_dev()
{
	"$NDCTL" destroy-namespace -f -b "$testbus" "$testdev"
}

daxctl_get_dev()
{
	"$NDCTL" list -n "$1" -X | jq -er '.[].daxregion.devices[0].chardev'
}

daxctl_get_mode()
{
	"$DAXCTL" list -d "$1" | jq -er '.[].mode'
}

set_online_policy()
{
	echo "online" > /sys/devices/system/memory/auto_online_blocks
}

unset_online_policy()
{
	echo "offline" > /sys/devices/system/memory/auto_online_blocks
}

save_online_policy()
{
	saved_policy="$(cat /sys/devices/system/memory/auto_online_blocks)"
}

restore_online_policy()
{
	echo "$saved_policy" > /sys/devices/system/memory/auto_online_blocks
}

daxctl_test()
{
	local daxdev

	daxdev=$(daxctl_get_dev "$testdev")
	test -n "$daxdev"

	# these tests need to run with kernel onlining policy turned off
	save_online_policy
	unset_online_policy
	"$DAXCTL" reconfigure-device -N -m system-ram "$daxdev"
	[[ $(daxctl_get_mode "$daxdev") == "system-ram" ]]
	"$DAXCTL" online-memory "$daxdev"
	"$DAXCTL" offline-memory "$daxdev"
	"$DAXCTL" reconfigure-device -m devdax "$daxdev"
	[[ $(daxctl_get_mode "$daxdev") == "devdax" ]]
	"$DAXCTL" reconfigure-device -m system-ram "$daxdev"
	[[ $(daxctl_get_mode "$daxdev") == "system-ram" ]]
	"$DAXCTL" reconfigure-device -f -m devdax "$daxdev"
	[[ $(daxctl_get_mode "$daxdev") == "devdax" ]]

	# fail 'ndctl-disable-namespace' while the devdax namespace is active
	# as system-ram. If this test fails, a reboot will be required to
	# recover from the resulting state.
	test -n "$testdev"
	"$DAXCTL" reconfigure-device -m system-ram "$daxdev"
	[[ $(daxctl_get_mode "$daxdev") == "system-ram" ]]
	if ! "$NDCTL" disable-namespace "$testdev"; then
		echo "disable-namespace failed as expected"
	else
		echo "disable-namespace succeeded, expected failure"
		echo "reboot required to recover from this state"
		return 1
	fi
	"$DAXCTL" reconfigure-device -f -m devdax "$daxdev"
	[[ $(daxctl_get_mode "$daxdev") == "devdax" ]]

	# this tests for reconfiguration failure if an online-policy is set
	set_online_policy
	: "This command is expected to fail:"
	if ! "$DAXCTL" reconfigure-device -N -m system-ram "$daxdev"; then
		echo "reconfigure failed as expected"
	else
		echo "reconfigure succeeded, expected failure"
		restore_online_policy
		return 1
	fi

	restore_online_policy
}

find_testdev
setup_dev
rc=1
daxctl_test
reset_dev
exit 0
