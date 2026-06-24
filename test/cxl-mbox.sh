#!/bin/bash -Ex
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2026 Nvidia Corporation. All rights reserved.

. "$(dirname "$0")"/common

BIN="$TEST_PATH"/cxl-mbox
rc=77
# 237 is -ENODEV
ERR_NODEV=237

trap 'err $LINENO' ERR

modprobe -r cxl_test
modprobe cxl_test

main()
{
	test -x "$BIN" || do_skip "no CXL mailbox test"

	rc=0
	"$BIN" || rc=$?

	echo "status: $rc"
	if [ "$rc" -eq "$ERR_NODEV" ]; then
		do_skip "no cxl_test memdev"
	elif [ "$rc" -ne 0 ]; then
		echo "fail: $LINENO" && exit 1
	fi

	rc=1
	check_dmesg "$LINENO"

	_cxl_cleanup
}

{
	main "$@"; exit "$?"
}
