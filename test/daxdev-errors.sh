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

DEV=""
NDCTL="../ndctl/ndctl"
DAXCTL="../daxctl/daxctl"
BUS="-b nfit_test.0"
BUS1="-b nfit_test.1"
json2var="s/[{}\",]//g; s/:/=/g"
rc=77

err() {
	echo "test/daxdev-errors: failed at line $1"
	exit $rc
}

check_min_kver()
{
	local ver="$1"
	: "${KVER:=$(uname -r)}"

	[ -n "$ver" ] || return 1
	[[ "$ver" == "$(echo -e "$ver\n$KVER" | sort -V | head -1)" ]]
}

check_min_kver "4.12" || { echo "kernel $KVER lacks dax dev error handling"; exit $rc; }

set -e
trap 'err $LINENO' ERR

# setup (reset nfit_test dimms)
modprobe nfit_test
$NDCTL disable-region $BUS all
$NDCTL zero-labels $BUS all
$NDCTL enable-region $BUS all

rc=1

query=". | sort_by(.available_size) | reverse | .[0].dev"
region=$($NDCTL list $BUS -t pmem -Ri | jq -r "$query")

json=$($NDCTL create-namespace $BUS -r $region -t pmem -m dax -a 4096)
chardev=$(echo $json | jq ". | select(.mode == \"dax\") | .daxregion.devices[0].chardev")

#{
#  "dev":"namespace6.0",
#  "mode":"dax",
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

json1=$($NDCTL list $BUS --mode=dax --namespaces)
eval $(echo $json1 | sed -e "$json2var")
nsdev=$dev

json1=$($NDCTL list $BUS)
eval $(echo $json1 | sed -e "$json2var")
busdev=$dev

# inject errors in the middle of the namespace
err_sector="$(((size/512) / 2))"
err_count=8
$NDCTL inject-error --block="$err_sector" --count=$err_count $nsdev

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

# cleanup
$NDCTL disable-region $BUS all
$NDCTL disable-region $BUS1 all
modprobe -r nfit_test

exit 0
