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

DEV=""
NDCTL="../ndctl/ndctl"
DAXCTL="../daxctl/daxctl"
BUS="-b nfit_test.0"
BUS1="-b nfit_test.1"
json2var="s/[{}\",]//g; s/:/=/g"
rc=77

. ./common

check_min_kver "4.13" || do_skip "may lack multi-dax support"

trap 'err $LINENO' ERR

# setup (reset nfit_test dimms)
modprobe nfit_test
$NDCTL disable-region $BUS all
$NDCTL zero-labels $BUS all
$NDCTL enable-region $BUS all
rc=1

query=". | sort_by(.available_size) | reverse | .[0].dev"
region=$($NDCTL list $BUS -t pmem -Ri | jq -r "$query")

json=$($NDCTL create-namespace $BUS -r $region -t pmem -m devdax -a 4096 -s 16M)
chardev1=$(echo $json | jq ". | select(.mode == \"devdax\") | .daxregion.devices[0].chardev")
json=$($NDCTL create-namespace $BUS -r $region -t pmem -m devdax -a 4096 -s 16M)
chardev2=$(echo $json | jq ". | select(.mode == \"devdax\") | .daxregion.devices[0].chardev")

# cleanup
$NDCTL disable-region $BUS all
$NDCTL disable-region $BUS1 all
modprobe -r nfit_test

exit 0
