#!/bin/bash -Ex
# SPDX-License-Identifier: GPL-2.0
# Copyright(c) 2018 Intel Corporation. All rights reserved.

[ -f "../ndctl/ndctl" ] && [ -x "../ndctl/ndctl" ] && ndctl="../ndctl/ndctl"
[ -f "./ndctl/ndctl" ] && [ -x "./ndctl/ndctl" ] && ndctl="./ndctl/ndctl"
[ -z "$ndctl" ] && echo "Couldn't find an ndctl binary" && exit 1
bus="nfit_test.0"
bus1="nfit_test.1"
json2var="s/[{}\",]//g; s/:/=/g"
rc=77
dev=""
image="update-fw.img"

. ./common

trap 'err $LINENO' ERR

reset()
{
	$ndctl disable-region -b "$bus" all
	$ndctl zero-labels -b "$bus" all
	$ndctl enable-region -b "$bus" all
	if [ -f $image ]; then
		rm -f $image
	fi
}

cleanup()
{
	$ndctl disable-region -b "$bus" all
	$ndctl disable-region -b "$bus1" all
	modprobe -r nfit_test
}

detect()
{
	dev=$($ndctl list -b "$bus" -D | jq .[0].dev | tr -d '"')
	[ -n "$dev" ] || err "$LINENO"
}

do_tests()
{
	truncate -s 196608 $image
	$ndctl update-firmware -f $image $dev
}

check_min_kver "4.16" || do_skip "may lack firmware update test handling"

modprobe nfit_test
rc=1
reset
rc=2
detect
do_tests
cleanup
exit 0
