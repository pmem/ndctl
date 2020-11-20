#!/bin/bash -Ex
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2018-2020 Intel Corporation. All rights reserved.

blockdev=""
rc=77

. ./common

set -e
trap 'err $LINENO' ERR

# setup (reset nfit_test dimms)
modprobe nfit_test
$NDCTL disable-region -b $NFIT_TEST_BUS0 all
$NDCTL zero-labels -b $NFIT_TEST_BUS0 all
$NDCTL enable-region -b $NFIT_TEST_BUS0 all

rc=1

# create a fsdax namespace and clear errors (if any)
dev="x"
json=$($NDCTL create-namespace -b $NFIT_TEST_BUS0 -t pmem -m raw)
eval "$(echo "$json" | json2var)"
[ $dev = "x" ] && echo "fail: $LINENO" && exit 1

$NDCTL disable-namespace $dev
# On broken kernels this reassignment of capacity triggers a warning
# with the following signature, and results in ENXIO.
#     WARNING: CPU: 11 PID: 1378 at drivers/nvdimm/label.c:721 __pmem_label_update+0x55d/0x570 [libnvdimm]
#     Call Trace:
#      nd_pmem_namespace_label_update+0xd6/0x160 [libnvdimm]
#      uuid_store+0x15c/0x170 [libnvdimm]
#      kernfs_fop_write+0xf0/0x1a0
#      __vfs_write+0x26/0x150
uuidgen > /sys/bus/nd/devices/$dev/uuid
$NDCTL enable-namespace $dev

_cleanup
exit 0
