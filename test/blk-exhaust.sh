#!/bin/bash -x
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2015-2020 Intel Corporation. All rights reserved.

set -e

rc=77

. $(dirname $0)/common

check_min_kver "4.11" || do_skip "may lack blk-exhaustion fix"

trap 'err $LINENO' ERR

# setup (reset nfit_test dimms)
modprobe nfit_test
$NDCTL disable-region -b $NFIT_TEST_BUS0 all
$NDCTL zero-labels -b $NFIT_TEST_BUS0 all
$NDCTL enable-region -b $NFIT_TEST_BUS0 all

# if the kernel accounting is correct we should be able to create two
# pmem and two blk namespaces on nfit_test.0
rc=1
$NDCTL create-namespace -b $NFIT_TEST_BUS0 -t pmem
$NDCTL create-namespace -b $NFIT_TEST_BUS0 -t pmem
$NDCTL create-namespace -b $NFIT_TEST_BUS0 -t blk -m raw
$NDCTL create-namespace -b $NFIT_TEST_BUS0 -t blk -m raw

# clearnup and exit
_cleanup

exit 0
