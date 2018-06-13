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

NDCTL="../ndctl/ndctl"
BUS="-b nfit_test.0"
BUS1="-b nfit_test.1"
rc=77

. ./common

check_min_kver "4.11" || do_skip "may not provide reliable isetcookie values"

trap 'err $LINENO' ERR

# setup (reset nfit_test dimms)
modprobe nfit_test
$NDCTL disable-region $BUS all
$NDCTL zero-labels $BUS all

# grab the largest pmem region on $BUS
query=". | sort_by(.available_size) | reverse | .[0].dev"
region=$($NDCTL list $BUS -t pmem -Ri | jq -r "$query")

# we assume that $region is comprised of 4 dimms
query=". | .regions[0].mappings | sort_by(.dimm) | .[].dimm"
dimms=$($NDCTL list -DRi -r $region | jq -r "$query" | xargs)
i=1
for d in $dimms
do
	$NDCTL write-labels $d -i nmem${i}.bin
	i=$((i+1))
done

$NDCTL enable-region $BUS all

len=$($NDCTL list -r $region -N | jq -r "length")

if [ -z $len ]; then
	rc=1
	echo "failed to find legacy isetcookie namespace"
	exit 1
fi

$NDCTL disable-region $BUS all
$NDCTL disable-region $BUS1 all
modprobe -r nfit_test

exit 0
