#!/bin/bash
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2023 Intel Corporation. All rights reserved.

. "$(dirname "$0")/common"

# Results expected
num_overflow_expected=1
num_fatal_expected=2
num_failure_expected=16
num_info_expected=3

set -ex

trap 'err $LINENO' ERR

check_prereq "jq"

modprobe -r cxl_test
modprobe cxl_test

dev_path="/sys/bus/platform/devices"

test_cxl_events()
{
	memdev="$1"

	if [ ! -f "${dev_path}/${memdev}/event_trigger" ]; then
		echo "TEST: Kernel does not support test event trigger"
		exit 77
	fi

	echo "TEST: triggering $memdev"
	echo 1 > "${dev_path}/${memdev}/event_trigger"
}

readarray -t memdevs < <("$CXL" list -b cxl_test -Mi | jq -r '.[].host')

echo "TEST: Prep event trace"
echo "" > /sys/kernel/tracing/trace
echo 1 > /sys/kernel/tracing/events/cxl/enable
echo 1 > /sys/kernel/tracing/tracing_on

test_cxl_events "${memdevs[0]}"

echo 0 > /sys/kernel/tracing/tracing_on

echo "TEST: Events seen"
trace_out=$(cat /sys/kernel/tracing/trace)

num_overflow=$(grep -c "cxl_overflow" <<< "${trace_out}")
num_fatal=$(grep -c "log=Fatal" <<< "${trace_out}")
num_failure=$(grep -c "log=Failure" <<< "${trace_out}")
num_info=$(grep -c "log=Informational" <<< "${trace_out}")
echo "     LOG     (Expected) : (Found)"
echo "     overflow      ($num_overflow_expected) : $num_overflow"
echo "     Fatal         ($num_fatal_expected) : $num_fatal"
echo "     Failure       ($num_failure_expected) : $num_failure"
echo "     Informational ($num_info_expected) : $num_info"

if [ "$num_overflow" -ne $num_overflow_expected ]; then
	err "$LINENO"
fi
if [ "$num_fatal" -ne $num_fatal_expected ]; then
	err "$LINENO"
fi
if [ "$num_failure" -ne $num_failure_expected ]; then
	err "$LINENO"
fi
if [ "$num_info" -ne $num_info_expected ]; then
	err "$LINENO"
fi

check_dmesg "$LINENO"

modprobe -r cxl_test
