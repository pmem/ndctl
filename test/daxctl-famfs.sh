#!/bin/bash -Ex
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2025 Micron Technology, Inc. All rights reserved.
#
# Test daxctl famfs mode transitions and mode detection

rc=77
. $(dirname $0)/common

trap 'cleanup $LINENO' ERR

# Use cxl-test module to get the DAX device of the CXL auto region,
# which also makes this test NON destructive.
#
# The $CXL list below is a delay because find_daxdev() was not
# finding the DAX region without it.
#
modprobe -r cxl-test
modprobe cxl-test
$CXL list

daxdev=""
original_mode=""

cleanup()
{
	printf "Error at line %d\n" "$1"
	# Try to restore to original mode if we know it
	if [[ $daxdev && $original_mode ]]; then
		"$DAXCTL" reconfigure-device -f -m "$original_mode" "$daxdev" 2>/dev/null || true
	fi
	exit $rc
}

# Check if fsdev_dax module is available
check_fsdev_dax()
{
	if modinfo fsdev_dax &>/dev/null; then
		return 0
	fi
	if grep -qF "fsdev_dax" "/lib/modules/$(uname -r)/modules.builtin" 2>/dev/null; then
		return 0
	fi
	printf "fsdev_dax module not available, skipping\n"
	exit 77
}

# Check if kmem module is available (needed for system-ram mode tests)
check_kmem()
{
	if modinfo kmem &>/dev/null; then
		return 0
	fi
	if grep -qF "kmem" "/lib/modules/$(uname -r)/modules.builtin" 2>/dev/null; then
		return 0
	fi
	printf "kmem module not available, skipping system-ram tests\n"
	return 1
}

# Find an existing dax device to test with
find_daxdev()
{
	# Look for any available dax device
	daxdev=$("$DAXCTL" list | jq -er '.[0].chardev // empty' 2>/dev/null) || true

	if [[ ! $daxdev ]]; then
		printf "No dax device found, skipping\n"
		exit 77
	fi

	# Save the original mode so we can restore it
	original_mode=$("$DAXCTL" list -d "$daxdev" | jq -er '.[].mode')

	printf "Found dax device: %s (current mode: %s)\n" "$daxdev" "$original_mode"
}

daxctl_get_mode()
{
	"$DAXCTL" list -d "$1" | jq -er '.[].mode'
}

# Ensure device is in devdax mode for testing
ensure_devdax_mode()
{
	local mode
	mode=$(daxctl_get_mode "$daxdev")

	if [[ "$mode" == "devdax" ]]; then
		return 0
	fi

	if [[ "$mode" == "system-ram" ]]; then
		printf "Device is in system-ram mode, attempting to convert to devdax...\n"
		"$DAXCTL" reconfigure-device -f -m devdax "$daxdev"
	elif [[ "$mode" == "famfs" ]]; then
		printf "Device is in famfs mode, converting to devdax...\n"
		"$DAXCTL" reconfigure-device -m devdax "$daxdev"
	else
		printf "Device is in unknown mode: %s\n" "$mode"
		return 1
	fi

	[[ $(daxctl_get_mode "$daxdev") == "devdax" ]]
}

#
# Test basic mode transitions involving famfs
#
test_famfs_mode_transitions()
{
	printf "\n=== Testing famfs mode transitions ===\n"

	# Ensure starting in devdax mode
	ensure_devdax_mode
	[[ $(daxctl_get_mode "$daxdev") == "devdax" ]]
	printf "Initial mode: devdax - OK\n"

	# Test: devdax -> famfs
	printf "Testing devdax -> famfs... "
	"$DAXCTL" reconfigure-device -m famfs "$daxdev"
	[[ $(daxctl_get_mode "$daxdev") == "famfs" ]]
	printf "OK\n"

	# Test: famfs -> famfs (re-enable in same mode)
	printf "Testing famfs -> famfs (re-enable)... "
	"$DAXCTL" reconfigure-device -m famfs "$daxdev"
	[[ $(daxctl_get_mode "$daxdev") == "famfs" ]]
	printf "OK\n"

	# Test: famfs -> devdax
	printf "Testing famfs -> devdax... "
	"$DAXCTL" reconfigure-device -m devdax "$daxdev"
	[[ $(daxctl_get_mode "$daxdev") == "devdax" ]]
	printf "OK\n"

	# Test: devdax -> devdax (re-enable in same mode)
	printf "Testing devdax -> devdax (re-enable)... "
	"$DAXCTL" reconfigure-device -m devdax "$daxdev"
	[[ $(daxctl_get_mode "$daxdev") == "devdax" ]]
	printf "OK\n"
}

#
# Test mode transitions with system-ram (requires kmem)
#
test_system_ram_transitions()
{
	printf "\n=== Testing system-ram transitions with famfs ===\n"

	# Ensure we start in devdax mode
	ensure_devdax_mode
	[[ $(daxctl_get_mode "$daxdev") == "devdax" ]]

	# Test: devdax -> system-ram
	printf "Testing devdax -> system-ram... "
	"$DAXCTL" reconfigure-device -N -m system-ram "$daxdev"
	[[ $(daxctl_get_mode "$daxdev") == "system-ram" ]]
	printf "OK\n"

	# Test: system-ram -> famfs should fail
	printf "Testing system-ram -> famfs (should fail)... "
	if "$DAXCTL" reconfigure-device -m famfs "$daxdev" 2>/dev/null; then
		printf "FAILED - should have been rejected\n"
		return 1
	fi
	printf "OK (correctly rejected)\n"

	# Test: system-ram -> devdax -> famfs (proper path)
	printf "Testing system-ram -> devdax -> famfs... "
	"$DAXCTL" reconfigure-device -f -m devdax "$daxdev"
	[[ $(daxctl_get_mode "$daxdev") == "devdax" ]]
	"$DAXCTL" reconfigure-device -m famfs "$daxdev"
	[[ $(daxctl_get_mode "$daxdev") == "famfs" ]]
	printf "OK\n"

	# Restore to devdax for subsequent tests
	"$DAXCTL" reconfigure-device -m devdax "$daxdev"
}

#
# Test JSON output shows correct mode
#
test_json_output()
{
	printf "\n=== Testing JSON output for mode field ===\n"

	# Test devdax mode in JSON
	ensure_devdax_mode
	printf "Testing JSON output for devdax mode... "
	mode=$("$DAXCTL" list -d "$daxdev" | jq -er '.[].mode')
	[[ "$mode" == "devdax" ]]
	printf "OK\n"

	# Test famfs mode in JSON
	"$DAXCTL" reconfigure-device -m famfs "$daxdev"
	printf "Testing JSON output for famfs mode... "
	mode=$("$DAXCTL" list -d "$daxdev" | jq -er '.[].mode')
	[[ "$mode" == "famfs" ]]
	printf "OK\n"

	# Restore to devdax
	"$DAXCTL" reconfigure-device -m devdax "$daxdev"
}

#
# Test error messages for invalid transitions
#
test_error_handling()
{
	printf "\n=== Testing error handling ===\n"

	# Ensure we're in famfs mode
	"$DAXCTL" reconfigure-device -m famfs "$daxdev"

	# Test that invalid mode is rejected
	printf "Testing invalid mode rejection... "
	if "$DAXCTL" reconfigure-device -m invalidmode "$daxdev" 2>/dev/null; then
		printf "FAILED - invalid mode should be rejected\n"
		return 1
	fi
	printf "OK (correctly rejected)\n"

	# Restore to devdax
	"$DAXCTL" reconfigure-device -m devdax "$daxdev"
}

#
# Main test sequence
#
main()
{
	check_fsdev_dax
	find_daxdev

	rc=1  # From here on, failures are real failures

	test_famfs_mode_transitions
	test_json_output
	test_error_handling

	# System-ram tests require kmem module
	if check_kmem; then
		# Save and disable online policy for system-ram tests
		saved_policy="$(cat /sys/devices/system/memory/auto_online_blocks)"
		echo "offline" > /sys/devices/system/memory/auto_online_blocks

		test_system_ram_transitions

		# Restore online policy
		echo "$saved_policy" > /sys/devices/system/memory/auto_online_blocks
	fi

	# Restore original mode
	printf "\nRestoring device to original mode: %s\n" "$original_mode"
	"$DAXCTL" reconfigure-device -f -m "$original_mode" "$daxdev"

	printf "\n=== All famfs tests passed ===\n"

	exit 0
}

main
