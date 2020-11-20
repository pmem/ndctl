#!/bin/bash
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2015-2020 Intel Corporation. All rights reserved.

. $(dirname $0)/common

rc=77

set -e

check_min_kver "4.11" || do_skip "kernel may lack device-dax fixes"

trap 'err $LINENO' ERR

check_prereq "fio"
if ! fio --enghelp | grep -q "dev-dax"; then
	echo "fio lacks dev-dax engine"
	exit 77
fi

dev=$(./dax-dev)
for align in 4k 2m 1g
do
	json=$($NDCTL create-namespace -m devdax -a $align -f -e $dev)
	chardev=$(echo $json | jq -r ". | select(.mode == \"devdax\") | .daxregion.devices[0].chardev")
	if [ align = "1g" ]; then
		bs="1g"
	else
		bs="2m"
	fi

	cat > fio.job <<- EOF
		[global]
		ioengine=dev-dax
		direct=0
		filename=/dev/${chardev}
		verify=crc32c
		bs=${bs}

		[write]
		rw=write
		runtime=5

		[read]
		stonewall
		rw=read
		runtime=5
	EOF

	rc=1
	fio fio.job 2>&1 | tee fio.log

	if grep -q "fio.*got signal" fio.log; then
		echo "test/device-dax-fio.sh: failed with align: $align"
		exit 1
	fi

	# revert namespace to raw mode
	json=$($NDCTL create-namespace -m raw -f -e $dev)
	eval $(json2var <<< "$json")
	[ $mode != "fsdax" ] && echo "fail: $LINENO" && exit 1
done

exit 0
