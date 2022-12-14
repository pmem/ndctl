#!/bin/bash
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2022 Intel Corporation. All rights reserved.

. $(dirname $0)/common

rc=77

set -ex

trap 'err $LINENO' ERR

check_prereq "jq"

modprobe -r cxl_test
modprobe cxl_test
rc=1

test_label_ops()
{
	nmem="$1"
	lsa=$(mktemp /tmp/lsa-$nmem.XXXX)
	lsa_read=$(mktemp /tmp/lsa-read-$nmem.XXXX)

	# determine LSA size
	"$NDCTL" read-labels -o "$lsa_read" "$nmem"
	lsa_size=$(stat -c %s "$lsa_read")

	dd "if=/dev/urandom" "of=$lsa" "bs=$lsa_size" "count=1"
	"$NDCTL" write-labels -i "$lsa" "$nmem"
	"$NDCTL" read-labels -o "$lsa_read" "$nmem"

	# compare what was written vs read
	diff "$lsa" "$lsa_read"

	# zero the LSA and test
	"$NDCTL" zero-labels "$nmem"
	dd "if=/dev/zero" "of=$lsa" "bs=$lsa_size" "count=1"
	"$NDCTL" read-labels -o "$lsa_read" "$nmem"
	diff "$lsa" "$lsa_read"

	# cleanup
	rm "$lsa" "$lsa_read"
}

test_label_ops_cxl()
{
	mem="$1"
	lsa_read=$(mktemp /tmp/lsa-read-$mem.XXXX)

	"$CXL" read-labels -o "$lsa_read" "$mem"
	rm "$lsa_read"
}

# test reading labels directly through cxl-cli
readarray -t mems < <("$CXL" list -b cxl_test -Mi | jq -r '.[].memdev')

for mem in ${mems[@]}; do
	test_label_ops_cxl "$mem"
done

# find nmem devices corresponding to cxl memdevs
readarray -t nmems < <("$NDCTL" list -b cxl_test -Di | jq -r '.[].dev')

for nmem in ${nmems[@]}; do
	test_label_ops "$nmem"
done

check_dmesg "$LINENO"

modprobe -r cxl_test
