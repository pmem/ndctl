#!/bin/bash
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2023 Intel Corporation. All rights reserved.

. $(dirname $0)/common

rc=77

set -ex

trap 'err $LINENO' ERR

check_prereq "jq"
check_prereq "dd"
check_prereq "sha256sum"

modprobe -r cxl_test
modprobe cxl_test
rc=1

mk_fw_file()
{
	size="$1"

	if [[ ! $size ]]; then
		err "$LINENO"
	fi
	if (( size > 64 )); then
		err "$LINENO"
	fi

	fw_file="$(mktemp -p /tmp fw_file_XXXX)"
	dd if=/dev/urandom of="$fw_file" bs=1M count="$size"
	echo "$fw_file"
}

find_memdevs()
{
	count="$1"

	if [[ ! $count ]]; then
		count=1
	fi

	"$CXL" list -M -b "$CXL_TEST_BUS" \
		| jq -r '.[] | select(.host | startswith("cxl_mem.")) | .memdev' \
		| head -"$count"
}

do_update_fw()
{
	"$CXL" update-firmware -b "$CXL_TEST_BUS" "$@"
}

wait_complete()
{
	mem="$1"  # single memdev, not a list
	max_wait="$2"  # in seconds
	waited=0

	while true; do
		json="$("$CXL" list -m "$mem" -F)"
		in_prog="$(jq -r '.[].firmware.fw_update_in_progress' <<< "$json")"
		if [[ $in_prog == "true" ]]; then
			sleep 1
			waited="$((waited + 1))"
			continue
		else
			break
		fi
		if (( waited == max_wait )); then
			echo "completion timeout for $mem"
			err "$LINENO"
		fi
	done
}

validate_json_state()
{
	json="$1"
	state="$2"

	while read -r in_prog_state; do
		if [[ $in_prog_state == $state ]]; then
			continue
		else
			echo "expected fw_update_in_progress:$state"
			err "$LINENO"
		fi
	done < <(jq -r '.[].firmware.fw_update_in_progress' <<< "$json")
}

validate_fw_update_in_progress()
{
	validate_json_state "$1" "true"
}

validate_fw_update_idle()
{
	validate_json_state "$1" "false"
}

validate_staged_slot()
{
	json="$1"
	slot="$2"

	while read -r staged_slot; do
		if [[ $staged_slot == $slot ]]; then
			continue
		else
			echo "expected staged_slot:$slot"
			err "$LINENO"
		fi
	done < <(jq -r '.[].firmware.staged_slot' <<< "$json")
}

check_sha()
{
	mem="$1"
	host=$($CXL list -m $mem | jq -r '.[].host')
	file="$2"
	csum_path="/sys/bus/platform/devices/${host}/fw_buf_checksum"

	mem_csum="$(cat "$csum_path")"
	file_csum="$(sha256sum "$file" | awk '{print $1}')"

	if [[ $mem_csum != $file_csum ]]; then
		echo "checksum failure for mem=$mem"
		err "$LINENO"
	fi
}

test_blocking_update()
{
	file="$(mk_fw_file 8)"
	mem="$(find_memdevs 1)"
	json=$(do_update_fw -F "$file" --wait "$mem")
	validate_fw_update_idle "$json"
	# cxl_test's starting slot is '2', so staged should be 3
	validate_staged_slot "$json" 3
	check_sha "$mem" "$file"
	rm "$file"
}

test_nonblocking_update()
{
	file="$(mk_fw_file 16)"
	mem="$(find_memdevs 1)"
	json=$(do_update_fw -F "$file" "$mem")
	validate_fw_update_in_progress "$json"
	wait_complete "$mem" 15
	validate_fw_update_idle "$("$CXL" list -m "$mem" -F)"
	check_sha "$mem" "$file"
	rm "$file"
}

test_multiple_memdev()
{
	num_mems=2

	file="$(mk_fw_file 16)"
	declare -a mems
	mems=( $(find_memdevs "$num_mems") )
	json="$(do_update_fw -F "$file" "${mems[@]}")"
	validate_fw_update_in_progress "$json"
	# use the in-band wait this time
	json="$(do_update_fw --wait "${mems[@]}")"
	validate_fw_update_idle "$json"
	for mem in ${mems[@]}; do
		check_sha "$mem" "$file"
	done
	rm "$file"
}

test_cancel()
{
	file="$(mk_fw_file 16)"
	mem="$(find_memdevs 1)"
	json=$(do_update_fw -F "$file" "$mem")
	validate_fw_update_in_progress "$json"
	do_update_fw --cancel "$mem"
	# cancellation is asynchronous, and the result looks the same as idle
	wait_complete "$mem" 15
	validate_fw_update_idle "$("$CXL" list -m "$mem" -F)"
	# no need to check_sha
	rm "$file"
}

test_blocking_update
test_nonblocking_update
test_multiple_memdev
test_cancel

check_dmesg "$LINENO"
modprobe -r cxl_test
