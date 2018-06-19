#!/bin/bash -Ex
# SPDX-License-Identifier: GPL-2.0
# Copyright(c) 2018 Intel Corporation. All rights reserved.

rc=77
dev=""
image="update-fw.img"

. ./common

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
	dev=$($NDCTL list -b $NFIT_TEST_BUS0 -D | jq .[0].dev | tr -d '"')
	[ -n "$dev" ] || err "$LINENO"
}

do_tests()
{
	truncate -s 196608 $image
	$NDCTL update-firmware -f $image $dev
}

check_min_kver "4.16" || do_skip "may lack firmware update test handling"

modprobe nfit_test
rc=1
reset
rc=2
detect
do_tests
_cleanup
exit 0
