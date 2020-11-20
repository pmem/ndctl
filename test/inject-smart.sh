#!/bin/bash -Ex
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2018-2020 Intel Corporation. All rights reserved.

rc=77
. $(dirname $0)/common
bus="$NFIT_TEST_BUS0"
inj_val="42"

trap 'err $LINENO' ERR

# sample json:
# {
#  "dev":"nmem0",
#  "id":"cdab-0a-07e0-ffffffff",
#  "handle":0,
#  "phys_id":0,
#  "health":{
#    "health_state":"non-critical",
#    "temperature_celsius":23,
#    "spares_percentage":75,
#    "alarm_temperature":true,
#    "alarm_spares":true,
#    "temperature_threshold":40,
#    "spares_threshold":5,
#    "life_used_percentage":5,
#    "shutdown_state":"clean"
#  }
#}

translate_field()
{
	local in="$1"

	case $in in
	media-temperature)
		echo "temperature_celsius"
		;;
	ctrl-temperature)
		echo "controller_temperature_celsius"
		;;
	spares)
		echo "spares_percentage"
		;;
	media-temperature-alarm)
		echo "alarm_temperature"
		;;
	ctrl-temperature-alarm)
		echo "alarm_controller_temperature"
		;;
	spares-alarm)
		echo "alarm_spares"
		;;
	media-temperature-threshold)
		echo "temperature_threshold"
		;;
	spares-threshold)
		echo "spares_threshold"
		;;
	unsafe-shutdown)
		echo "shutdown_state"
		;;
	fatal)
		echo "health_state"
		;;
	*)
		# passthrough
		echo "$in"
		return
		;;
	esac
}

translate_val()
{
	local in="$1"

	case $in in
	dirty)
		;&
	fatal)
		;&
	true)
		echo "1"
		;;
	non-critical)
		;&
	clean)
		;&
	false)
		echo "0"
		;;
	*)
		# passthrough
		echo "$in"
		;;
	esac
}

get_field()
{
	local field="$1"
	local smart_listing="$(translate_field $field)"

	json="$($NDCTL list -b $bus -d $dimm -H)"
	val="$(jq -r ".[].dimms[].health.$smart_listing" <<< $json)"
	val="$(translate_val $val)"
	echo $val
}

verify()
{
	local field="$1"
	local val="$2"

	[[ "$val" == "$(get_field $field)" ]]
}

test_field()
{
	local field="$1"
	local val="$2"
	local op="$3"
	local old_val=""

	if [ -n "$val" ]; then
		inj_opt="--${field}=${val}"
	else
		inj_opt="--${field}"
	fi

	old_val=$(get_field $field)
	if [[ "$old_val" == "0" || "$old_val" == "1" ]]; then
		val=$(((old_val + 1) % 2))
	fi
	$NDCTL inject-smart -b $bus $dimm $inj_opt
	verify $field $val

	if [[ "$op" != "thresh" ]]; then
		$NDCTL inject-smart -b $bus --${field}-uninject $dimm
		verify $field $old_val
	fi
}

do_tests()
{
	local fields_val=(media-temperature spares)
	local fields_bool=(unsafe-shutdown fatal)
	local fields_thresh=(media-temperature-threshold spares-threshold)
	local field=""

	$NDCTL inject-smart -b $bus --uninject-all $dimm

	# start tests
	for field in "${fields_val[@]}"; do
		test_field $field $inj_val
	done

	for field in "${fields_bool[@]}"; do
		test_field $field
	done

	for field in "${fields_thresh[@]}"; do
		test_field $field $inj_val "thresh"
	done
}

check_min_kver "4.19" || do_skip "kernel $KVER may not support smart (un)injection"
modprobe nfit_test
rc=1

jlist=$(./list-smart-dimm -b $bus)
dimm="$(jq '.[]."dev"?, ."dev"?' <<< $jlist | sort | head -1 | xargs)"
test -n "$dimm"

do_tests
_cleanup
exit 0
