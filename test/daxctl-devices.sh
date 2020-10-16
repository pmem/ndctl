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

daxctl_test()
{
	local daxdev

	daxdev=$(daxctl_get_dev "$testdev")
	test -n "$daxdev"

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
}

find_testdev
setup_dev
rc=1
daxctl_test
reset_dev
exit 0
