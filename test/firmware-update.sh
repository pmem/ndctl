#!/bin/bash -Ex
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2018-2020 Intel Corporation. All rights reserved.

rc=77
dev=""
image="update-fw.img"

. $(dirname $0)/common

trap 'err $LINENO' ERR

reset()
{
	$NDCTL disable-region -b $NFIT_TEST_BUS0 all
	$NDCTL zero-labels -b $NFIT_TEST_BUS0 all
	$NDCTL enable-region -b $NFIT_TEST_BUS0 all
	if [ -f $image ]; then
		rm -f $image
	fi
}

detect()
{
	$NDCTL wait-scrub $NFIT_TEST_BUS0
	fwa=$($NDCTL list -b $NFIT_TEST_BUS0 -F | jq -r '.[0].firmware.activate_method')
	[ $fwa = "suspend" ] || err "$LINENO"
	count=$($NDCTL list -b $NFIT_TEST_BUS0 -D | jq length)
	[ $((count)) -eq 4 ] || err "$LINENO"
}

do_tests()
{
	# create a dummy image file, try to update all 4 dimms on
	# nfit_test.0, validate that all get staged, validate that all
	# but one get armed relative to an overflow error.
	truncate -s 196608 $image
	json=$($NDCTL update-firmware -b $NFIT_TEST_BUS0 -f $image all)
	count=$(jq 'map(select(.firmware.activate_state == "armed")) | length' <<< $json)
	[ $((count)) -eq 3 ] || err "$LINENO"
	count=$(jq 'map(select(.firmware.activate_state == "idle")) | length' <<< $json)
	[ $((count)) -eq 1 ] || err "$LINENO"

	# validate that the overflow dimm can be force armed
	dev=$(jq -r '.[] | select(.firmware.activate_state == "idle").dev' <<< $json)
	json=$($NDCTL update-firmware -b $NFIT_TEST_BUS0 $dev -A --force)
	state=$(jq -r '.[0].firmware.activate_state' <<< $json)
	[ $state = "armed" ] || err "$LINENO"

	# validate that the bus indicates overflow
	fwa=$($NDCTL list -b $NFIT_TEST_BUS0 -F | jq -r '.[0].firmware.activate_state')
	[ $fwa = "overflow" ] || err "$LINENO"

	# validate that all devices can be disarmed, and the bus goes idle
	json=$($NDCTL update-firmware -b $NFIT_TEST_BUS0 -D all)
	count=$(jq 'map(select(.firmware.activate_state == "idle")) | length' <<< $json)
	[ $((count)) -eq 4 ] || err "$LINENO"
	fwa=$($NDCTL list -b $NFIT_TEST_BUS0 -F | jq -r '.[0].firmware.activate_state')
	[ $fwa = "idle" ] || err "$LINENO"

	# re-arm all DIMMs
	json=$($NDCTL update-firmware -b $NFIT_TEST_BUS0 -A --force all)
	count=$(jq 'map(select(.firmware.activate_state == "armed")) | length' <<< $json)
	[ $((count)) -eq 4 ] || err "$LINENO"

	# trigger activation via suspend
	json=$($NDCTL activate-firmware -v $NFIT_TEST_BUS0)
	idle_count=$(jq '.[].dimms | map(select(.firmware.activate_state == "idle")) | length' <<< $json)
	busy_count=$(jq '.[].dimms | map(select(.firmware.activate_state == "busy")) | length' <<< $json)
	[ $((idle_count)) -eq 4 -o $((busy_count)) -eq 4 ] || err "$LINENO"
}

check_min_kver "4.16" || do_skip "may lack firmware update test handling"

modprobe nfit_test
reset
detect
rc=1
do_tests
rm -f $image
_cleanup
exit 0
