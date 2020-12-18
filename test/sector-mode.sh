#!/bin/bash -x
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2015-2020 Intel Corporation. All rights reserved.

rc=77

. $(dirname $0)/common

set -e
trap 'err $LINENO' ERR

# setup (reset nfit_test dimms)
modprobe nfit_test
$NDCTL disable-region -b $NFIT_TEST_BUS0 all
$NDCTL zero-labels -b $NFIT_TEST_BUS0 all
$NDCTL enable-region -b $NFIT_TEST_BUS0 all

$NDCTL disable-region -b $NFIT_TEST_BUS1 all
$NDCTL zero-labels -b $NFIT_TEST_BUS1 all
$NDCTL enable-region -b $NFIT_TEST_BUS1 all

rc=1
query=". | sort_by(.size) | reverse | .[0].dev"
NAMESPACE=$($NDCTL list -b $NFIT_TEST_BUS1 -N | jq -r "$query")
REGION=$($NDCTL list -R --namespace=$NAMESPACE | jq -r "(.[]) | .dev")
echo 0 > /sys/bus/nd/devices/$REGION/read_only
$NDCTL create-namespace --no-autolabel -e $NAMESPACE -m sector -f -l 4K
$NDCTL create-namespace --no-autolabel -e $NAMESPACE -m dax -f -a 4K
$NDCTL create-namespace --no-autolabel -e $NAMESPACE -m sector -f -l 4K

_cleanup

exit 0
