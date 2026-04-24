#!/bin/bash -E
# SPDX-License-Identifier: GPL-2.0

dev=""
mode=""
size=""
sector_size=""
blockdev=""
rc=77

. $(dirname $0)/common

trap 'err $LINENO' ERR

check_min_kver "7.2" || do_skip "known BTT lane race before fix"

# Stress BTT I/O under contention to exercise lane acquisition races.
# Background readers contend for lanes while CPU loops increase
# preemption pressure.

create() {
	json=$($NDCTL create-namespace -b "$NFIT_TEST_BUS0" -t pmem -m sector)
	rc=2
	eval "$(echo "$json" | json2var)"
	[ -n "$dev" ] || err "$LINENO"
	[ "$mode" = "sector" ] || err "$LINENO"
	[ -n "$size" ] || err "$LINENO"
	[ -n "$sector_size" ] || err "$LINENO"
	[ -n "$blockdev" ] || err "$LINENO"
	[ "$size" -gt 0 ] || err "$LINENO"
}

# Start background workers:
#   - readers contend for lanes
#   - CPU loops increase preemption
start_bg_workers() {
	local ncpus
	ncpus=$(nproc)
	local nworkers=$((ncpus / 2))

	# Ensure at least one worker, cap to limit runtime noise
	[ $nworkers -lt 1 ] && nworkers=1
	[ $nworkers -gt 8 ] && nworkers=8

	BG_PIDS=()
	local i
	for i in $(seq 1 $nworkers); do
		# Reader: contends for lanes (use O_DIRECT to avoid page cache)
		(while :; do
			dd if=/dev/"$blockdev" of=/dev/null \
				bs="$sector_size" count=256 \
				iflag=direct >/dev/null 2>&1 || true
		done) &
		BG_PIDS+=($!)

		# CPU hog: increase preemption
		(while :; do :; done) &
		BG_PIDS+=($!)
	done
	echo "started $nworkers readers + $nworkers CPU hogs"
}

stop_bg_workers() {
	local pid
	for pid in "${BG_PIDS[@]}"; do
		kill "$pid" 2>/dev/null || true
	done
	wait "${BG_PIDS[@]}" 2>/dev/null || true
	BG_PIDS=()
}

# Write, read, and verify data
do_io_verify() {
	dd if=/dev/urandom of=test-bin \
		bs="$sector_size" count=$((size / sector_size)) >/dev/null 2>&1
	dd if=test-bin of=/dev/"$blockdev" \
		bs="$sector_size" count=$((size / sector_size)) >/dev/null 2>&1
	dd if=/dev/"$blockdev" of=test-bin-read \
		bs="$sector_size" count=$((size / sector_size)) >/dev/null 2>&1
	diff test-bin test-bin-read
	rm -f test-bin*
}

# Run verification under contention
test_io_stress() {
	local iterations=${1:-20}
	echo "=== ${FUNCNAME[0]} ($iterations iterations) ==="

	start_bg_workers
	trap 'stop_bg_workers; err $LINENO' ERR

	local i
	for i in $(seq 1 "$iterations"); do
		echo "--- iteration $i/$iterations ---"
		do_io_verify
	done

	stop_bg_workers
	trap 'err $LINENO' ERR
}

modprobe nfit_test
rc=1
reset && create

# 30 iterations balances runtime and reproduction probability
test_io_stress 30

reset
_cleanup
exit 0
