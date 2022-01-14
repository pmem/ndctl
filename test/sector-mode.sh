#!/bin/bash -x
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2015-2020 Intel Corporation. All rights reserved.

rc=77

. $(dirname $0)/common

check_prereq "jq"

set -e
trap 'err $LINENO' ERR

ALIGN_SIZE=`getconf PAGESIZE`

# setup (reset nfit_test dimms)
modprobe nfit_test
reset
reset1

rc=1
query=". | sort_by(.available_size) | reverse | .[0].dev"
REGION=$($NDCTL list -R -b $NFIT_TEST_BUS1 | jq -r "$query")
echo 0 > /sys/bus/nd/devices/$REGION/read_only
echo $ALIGN_SIZE > /sys/bus/nd/devices/$REGION/align
NAMESPACE=$($NDCTL create-namespace --no-autolabel -r $REGION -m sector -f -l 4K | jq -r ".dev")
$NDCTL create-namespace --no-autolabel -e $NAMESPACE -m dax -f -a $ALIGN_SIZE
$NDCTL create-namespace --no-autolabel -e $NAMESPACE -m sector -f -l 4K

_cleanup

exit 0
