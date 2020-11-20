#!/bin/bash -x
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2015-2020 Intel Corporation. All rights reserved.

set -e

rc=77

. $(dirname $0)/common

check_min_kver "4.12" || do_skip "lacks dax dev error handling"

trap 'err $LINENO' ERR

# setup (reset nfit_test dimms)
modprobe nfit_test
$NDCTL disable-region -b $NFIT_TEST_BUS0 all
$NDCTL zero-labels -b $NFIT_TEST_BUS0 all
$NDCTL enable-region -b $NFIT_TEST_BUS0 all

rc=1

query=". | sort_by(.available_size) | reverse | .[0].dev"
region=$($NDCTL list -b $NFIT_TEST_BUS0 -t pmem -Ri | jq -r "$query")

json=$($NDCTL create-namespace -b $NFIT_TEST_BUS0 -r $region -t pmem -m devdax -a 4096)
chardev=$(echo $json | jq ". | select(.mode == \"devdax\") | .daxregion.devices[0].chardev")

#{
#  "dev":"namespace6.0",
#  "mode":"devdax",
#  "size":64004096,
#  "uuid":"83a925dd-42b5-4ac6-8588-6a50bfc0c001",
#  "daxregion":{
#    "id":6,
#    "size":64004096,
#    "align":4096,
#    "devices":[
#      {
#        "chardev":"dax6.0",
#        "size":64004096
#      }
#    ]
#  }
#}

json1=$($NDCTL list -b $NFIT_TEST_BUS0 --mode=devdax --namespaces)
eval $(echo $json1 | json2var)
nsdev=$dev

json1=$($NDCTL list -b $NFIT_TEST_BUS0)
eval $(echo $json1 | json2var)
busdev=$dev

# inject errors in the middle of the namespace
err_sector="$(((size/512) / 2))"
err_count=8
if ! read sector len < /sys/bus/nd/devices/$region/badblocks; then
	$NDCTL inject-error --block="$err_sector" --count=$err_count $nsdev
fi

read sector len < /sys/bus/nd/devices/$region/badblocks
echo "sector: $sector len: $len"

# run the daxdev-errors test
test -x ./daxdev-errors
./daxdev-errors $busdev $region

# check badblocks, should be empty
if read sector len < /sys/bus/platform/devices/nfit_test.0/$busdev/$region/badblocks; then
	echo "badblocks empty, expected"
fi
[ -n "$sector" ] && echo "fail: $LINENO" && exit 1

_cleanup

exit 0
