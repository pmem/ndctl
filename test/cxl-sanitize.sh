#!/bin/bash
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2022 Intel Corporation. All rights reserved.

. $(dirname $0)/common

rc=77

set -ex

trap 'err $LINENO' ERR

check_prereq "jq"

modprobe -r cxl_test
modprobe cxl_test
rc=1

# THEORY OF OPERATION: Find a memdev with programmed decoders, validate
# that sanitize requests fail. Find a memdev without programmed
# decoders, validate that submission succeeds, and validate that the
# notifier fires.

mem_to_host()
{
	host=$("$CXL" list -m $1 | jq -r '.[].host')
	echo $host
}

set_timeout()
{
	host=$(mem_to_host $1)
	echo $2 > /sys/bus/platform/devices/$host/sanitize_timeout
}

# find all memdevs
readarray -t all_mem < <("$CXL" list -b cxl_test -M | jq -r '.[].memdev')

# try to sanitize an active memdev
readarray -t active_mem < <("$CXL" list -b cxl_test -RT | jq -r '.[].mappings[].memdev')
count=${#active_mem[@]}
((count > 0)) || err $LINENO

# set timeout to 2 seconds
set_timeout ${active_mem[0]} 2000

# sanitize with an active memdev should fail
echo 1 > /sys/bus/cxl/devices/${active_mem[0]}/security/sanitize && err $LINENO

# find an inactive mem
inactive=""
for mem in ${all_mem[@]}; do
	inactive=$mem
	for active in ${active_mem[@]}; do
		if [ $mem = $active ]; then
			inactive=""
		fi
	done
	if [ -z $inactive ]; then
		continue;
	fi
	break
done
[ -z $inactive ] && err $LINENO

# kickoff a background sanitize and make sure the wait takes a couple
# secounds
set_timeout $inactive 3000
start=$SECONDS
echo 1 > /sys/bus/cxl/devices/${inactive}/security/sanitize &
"$CXL" wait-sanitize $inactive || err $LINENO
((SECONDS > start + 2)) || err $LINENO

check_dmesg "$LINENO"

modprobe -r cxl_test
