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

NDCTL="../ndctl/ndctl"
BUS="-b nfit_test.0"
BUS1="-b nfit_test.1"
TEST=$0
rc=77

err() {
	echo "$TEST: failed at line $1"
	exit $rc
}

set -e
trap 'err $LINENO' ERR

# setup (reset nfit_test dimms)
modprobe nfit_test
$NDCTL disable-region $BUS all
$NDCTL zero-labels $BUS all
$NDCTL enable-region $BUS all

$NDCTL disable-region $BUS1 all
if $NDCTL zero-labels $BUS1 all; then
	echo "DIMMs on $BUS1 support labels, skip..."
	$NDCTL enable-region $BUS1 all
	false
fi
$NDCTL enable-region $BUS1 all

rc=1
query=". | sort_by(.size) | reverse | .[0].dev"
NAMESPACE=$($NDCTL list $BUS1 -N | jq -r "$query")
REGION=$($NDCTL list -R --namespace=$NAMESPACE | jq -r ".dev")
echo 0 > /sys/bus/nd/devices/$REGION/read_only
$NDCTL create-namespace -e $NAMESPACE -m sector -f -l 4K

$NDCTL disable-region $BUS all
$NDCTL disable-region $BUS1 all
modprobe -r nfit_test

exit 0
