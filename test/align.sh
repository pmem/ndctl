#!/bin/bash -x
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2015-2020 Intel Corporation. All rights reserved.

. $(dirname $0)/common

rc=77
cleanup() {
	echo "align.sh: failed at line $1"
	if [ "x$region" != "x" -a x$save_align != "x" ]; then
		echo $save_align > $region_path/align
	fi

	if [ "x$ns1" != "x" ]; then
		$NDCTL destroy-namespace -f $ns1
	fi
	if [ "x$ns2" != "x" ]; then
		$NDCTL destroy-namespace -f $ns2
	fi

	exit $rc
}

is_aligned() {
	val=$1
	align=$2

	if [ $((val & (align - 1))) -eq 0 ]; then
		return 0
	fi
	return 1
}

set -e
trap 'err $LINENO cleanup' ERR

find_region()
{
	$NDCTL list -R -b ACPI.NFIT | jq -r '[.[] | select(.available_size == .size)][0] | .dev'
}

region=$(find_region)
if [ "x$region" = "xnull"  ]; then
	# this is destructive
	$NDCTL disable-region -b ACPI.NFIT all
	$NDCTL init-labels -f -b ACPI.NFIT all
	$NDCTL enable-region -b ACPI.NFIT all
fi
region=$(find_region)
if [ "x$region" = "xnull"  ]; then
	unset $region
	echo "unable to find empty region"
	false
fi

region_path="/sys/bus/nd/devices/$region"
save_align=$(cat $region_path/align)

# check that the region is 1G aligned
resource=$(cat $region_path/resource)
is_aligned $resource $((1 << 30)) || (echo "need a 1GB aligned namespace to test alignment conditions" && false)

rc=1

# check that start-aligned, but end-misaligned namespaces can be created
# and probed
echo 4096 > $region_path/align
SIZE=$(((1<<30) + (8<<10)))
json=$($NDCTL create-namespace -r $region -s $SIZE -m fsdax -a 4K)
eval $(json2var <<< "$json")
$NDCTL disable-namespace $dev
$NDCTL enable-namespace $dev
ns1=$dev

# check that start-misaligned namespaces can't be created until the
# region alignment is set to a compatible value.
# Note the namespace capacity alignment requirement in this case is
# SUBSECTION_SIZE (2M) as the data alignment can be satisfied with
# metadata padding.
json=$($NDCTL create-namespace -r $region -s $SIZE -m fsdax -a 4K -f) || status="failed"
if [ $status != "failed" ]; then
	echo "expected namespace creation failure"
	eval $(json2var <<< "$json")
	$NDCTL destroy-namespace -f $dev
	false
fi

# check that start-misaligned namespaces can't be probed. Since the
# kernel does not support creating this misalignment, force it with a
# custom info-block
json=$($NDCTL create-namespace -r $region -s $SIZE -m raw)
eval $(json2var <<< "$json")

$NDCTL disable-namespace $dev
$NDCTL write-infoblock $dev -a 4K
$NDCTL enable-namespace $dev || status="failed"

if [ $status != "failed" ]; then
	echo "expected namespace enable failure"
	$NDCTL destroy-namespace -f $dev
	false
fi
ns2=$dev

# check that namespace with proper inner padding can be enabled, even
# though non-zero start_pad namespaces don't support dax
$NDCTL write-infoblock $ns2 -a 4K -O 8K
$NDCTL enable-namespace $ns2
$NDCTL destroy-namespace $ns2 -f
unset ns2

# check that all namespace alignments can be created with the region
# alignment at a compatible value
SIZE=$((2 << 30))
echo $((16 << 20)) > $region_path/align
for i in $((1 << 30)) $((2 << 20)) $((4 << 10))
do
	json=$($NDCTL create-namespace -r $region -s $SIZE -m fsdax -a $i)
	eval $(json2var <<< "$json")
	ns2=$dev
	$NDCTL disable-namespace $dev
	$NDCTL enable-namespace $dev
	$NDCTL destroy-namespace $dev -f
	unset ns2
done

# final cleanup
$NDCTL destroy-namespace $ns1 -f
exit 0
