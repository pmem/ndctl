#!/bin/bash -x

# Copyright(c) 2015-2017 Intel Corporation. All rights reserved.
# 
# This program is free software; you can redistribute it and/or modify it
# under the terms of version 2 of the GNU General Public License as
# published by the Free Software Foundation.
# 
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.

set -e

rc=77

. ./common

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
