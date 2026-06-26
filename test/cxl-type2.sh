#!/bin/bash
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2026 Intel Corporation. All rights reserved.

. "$(dirname "$0")"/common

rc=77
set -ex
trap 'err $LINENO' ERR
check_prereq "jq"

remove_kmod() {
	modprobe -r cxl_test
}

load_kmod() {
	modprobe cxl_test type2_test=1
}

init_check() {
	load_kmod
	[ -f /sys/module/cxl_test/parameters/type2_test ] || \
		do_skip "cxl_test type2_test module param not available"
	region=$("$CXL" list -b "$CXL_TEST_BUS" -R | jq -r '.[0].region // empty')
	[ -n "$region" ] || err "$LINENO"
}

# Test rootport disable/enable case
cycle_root_port() {
	echo "Testing root port disable/enable"
	memdev=$("$CXL" list -b "$CXL_TEST_BUS" -M | jq -r '.[0].memdev // empty')
	[ -n "$memdev" ] || err "$LINENO"
	port=$("$CXL" list -b "$CXL_TEST_BUS" -m "$memdev" -P | jq -r '.[0].port // empty')
	[ -n "$port" ] || err "$LINENO"

	"$CXL" disable-port "$port" -f
	region=$("$CXL" list -b "$CXL_TEST_BUS" -R | jq -r '.[0].region // empty')
	[ -z "$region" ] || err "$LINENO"

	"$CXL" enable-port "$port"
	echo cxl_type2_accel.0 > /sys/bus/platform/drivers/cxl_mock_accel/bind
	region=$("$CXL" list -b "$CXL_TEST_BUS" -R | jq -r '.[0].region // empty')
	[ -n "$region" ] || err "$LINENO"
}

# Test rebind device driver
cycle_pdev_driver() {
	echo "Testing device driver unbind/bind"
	region=$("$CXL" list -b "$CXL_TEST_BUS" -R | jq -r '.[0].region // empty')
	[ -n "$region" ] || err "$LINENO"
	echo cxl_type2_accel.0 > /sys/bus/platform/drivers/cxl_mock_accel/unbind
	region=$("$CXL" list -b "$CXL_TEST_BUS" -R | jq -r '.[0].region // empty')
	[ -z "$region" ] || err "$LINENO"
	echo cxl_type2_accel.0 > /sys/bus/platform/drivers/cxl_mock_accel/bind
	region=$("$CXL" list -b "$CXL_TEST_BUS" -R | jq -r '.[0].region // empty')
	[ -n "$region" ] || err "$LINENO"
}

# Test memdev removal with CXL CLI
test_dev_removal() {
	echo "Testing device removal with CXL CLI"
	region=$("$CXL" list -b "$CXL_TEST_BUS" -R | jq -r '.[0].region // empty')
	[ -n "$region" ] || err "$LINENO"
	"$CXL" disable-region "$region"
	region=$("$CXL" list -b "$CXL_TEST_BUS" -R | jq -r '.[0].region // empty')
	[ -z "$region" ] || err "$LINENO"

	# type2 region is auto region and cannot be destroyed
	region=$("$CXL" list -b "$CXL_TEST_BUS" -Ri | jq -r '.[0].region // empty')
	[ -n "$region" ] || err "$LINENO"
	"$CXL" destroy-region "$region" || true
	region=$("$CXL" list -b "$CXL_TEST_BUS" -Ri | jq -r '.[0].region // empty')
	[ -n "$region" ] || err "$LINENO"

	# Do it directly via sysfs since CXL CLI has checks that will skip sysfs
	echo "0" | tee /sys/bus/cxl/devices/"$region"/commit || true
	region=$("$CXL" list -b "$CXL_TEST_BUS" -Ri | jq -r '.[0].region // empty')
	[ -n "$region" ] || err "$LINENO"

	# Make sure there's no delete_region for a type2 root decoder
	rd=$("$CXL" list -b "$CXL_TEST_BUS" -r"$region" -D | jq -r '.[0]."root decoders"[0].decoder')
	[ -n "$rd" ] || err "$LINENO"
	[ ! -f /sys/bus/cxl/devices/"$rd"/delete_region ] || err "$LINENO"

	memdev=$("$CXL" list -b "$CXL_TEST_BUS" -M | jq -r '.[0].memdev // empty')
	[ -n "$memdev" ] || err "$LINENO"

	# memdev is not expected to be removed because region can't be destroyed
	"$CXL" disable-memdev "$memdev" || true
	memdev=$("$CXL" list -b "$CXL_TEST_BUS" -M | jq -r '.[0].memdev // empty')
	[ -n "$memdev" ] || err "$LINENO"
}

remove_kmod
init_check
rc=1

cycle_root_port
cycle_pdev_driver
test_dev_removal

check_dmesg "$LINENO"
remove_kmod
