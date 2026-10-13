#!/bin/bash
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2026 Intel Corporation. All rights reserved.

# Run CXL KUnit suites through the ndctl test harness.
# Discover test modules by name and check their KTAP results in debugfs.
# See Documentation/driver-api/cxl/testing.rst in the kernel.

# shellcheck disable=SC1091 # common is sourced from the test directory
. "$(dirname "$0")"/common

kunit_debugfs=/sys/kernel/debug/kunit

mapfile -t modules < <(find /lib/modules/"$(uname -r)" \
	-name 'cxl_*_kunit.ko*' -printf '%f\n' | sed 's/\.ko.*$//' | sort)

[ ${#modules[@]} -gt 0 ] ||
	do_skip "test requires CONFIG_CXL_KUNIT_TEST=m"

unload_modules() {
	local mod

	for mod in "${modules[@]}"; do
		modprobe -r "$mod" 2>/dev/null || true
	done
}

trap unload_modules EXIT
trap 'err $LINENO' ERR
set -ex
# shellcheck disable=SC2034 # rc is the exit status used by err()
rc=1

# Force test suites to rerun on module load
unload_modules

loaded=()
summary=()
nr_failed=0

for mod in "${modules[@]}"; do
	if modprobe "$mod"; then
		loaded+=("$mod")
	else
		summary+=("$mod: FAIL, module did not load")
		nr_failed=$((nr_failed + 1))
	fi
done

[ -d "$kunit_debugfs" ] ||
	do_skip "test requires CONFIG_KUNIT_DEBUGFS=y"

for mod in "${loaded[@]}"; do
	nr_suites=0

	for results in "$kunit_debugfs"/cxl_*/results; do
		[ -f "$results" ] || continue
		grep -qE "^[[:space:]]*# module: $mod\$" "$results" || continue

		suite=$(basename "$(dirname "$results")")
		nr_suites=$((nr_suites + 1))
		cat "$results"

		# Match the suite result, not individual test cases
		if grep -qE "^ok [0-9]+ $suite\$" "$results"; then
			summary+=("$mod: suite $suite PASS")
		else
			summary+=("$mod: suite $suite FAIL")
			nr_failed=$((nr_failed + 1))
		fi
	done

	if [ "$nr_suites" -eq 0 ]; then
		summary+=("$mod: FAIL, no suite results in $kunit_debugfs")
		nr_failed=$((nr_failed + 1))
	fi
done

set +x
echo "CXL KUnit summary:"
printf '  %s\n' "${summary[@]}"
set -x

[ "$nr_failed" -eq 0 ] || err "$LINENO"

echo "CXL KUnit: all suites from ${#modules[@]} modules passed"
