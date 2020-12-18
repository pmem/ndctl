#!/bin/bash -x
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2015-2020 Intel Corporation. All rights reserved.

set -e

rc=77

BASE=$(dirname $0)
. $BASE/common

check_min_kver "4.11" || do_skip "may not provide reliable isetcookie values"

trap 'err $LINENO' ERR

# setup (reset nfit_test dimms)
modprobe nfit_test
$NDCTL disable-region -b $NFIT_TEST_BUS0 all
$NDCTL zero-labels -b $NFIT_TEST_BUS0 all

# grab the largest pmem region on -b $NFIT_TEST_BUS0
query=". | sort_by(.available_size) | reverse | .[0].dev"
region=$($NDCTL list -b $NFIT_TEST_BUS0 -t pmem -Ri | jq -r "$query")

# we assume that $region is comprised of 4 dimms
query=". | .regions[0].mappings | sort_by(.dimm) | .[].dimm"
dimms=$($NDCTL list -DRi -r $region | jq -r "$query" | xargs)
i=1
for d in $dimms
do
	$NDCTL write-labels $d -i $BASE/nmem${i}.bin
	i=$((i+1))
done

$NDCTL enable-region -b $NFIT_TEST_BUS0 all

len=$($NDCTL list -r $region -N | jq -r "length")

if [ -z $len ]; then
	rc=1
	echo "failed to find legacy isetcookie namespace"
	exit 1
fi

_cleanup

exit 0
