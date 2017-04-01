#!/bin/bash -x
NDCTL="../ndctl/ndctl"
BUS="-b nfit_test.0"
BUS1="-b nfit_test.1"
rc=77

set -e

err() {
	echo "test/label-compat.sh: failed at line $1"
	exit $rc
}

check_min_kver()
{
	local ver="$1"
	: "${KVER:=$(uname -r)}"

	[ -n "$ver" ] || return 1
	[[ "$ver" == "$(echo -e "$ver\n$KVER" | sort -V | head -1)" ]]
}

check_min_kver "4.11" || { echo "kernel $KVER may lack blk-exhaustion fix"; exit $rc; }

set -e
trap 'err $LINENO' ERR

# setup (reset nfit_test dimms)
modprobe nfit_test
$NDCTL disable-region $BUS all
$NDCTL zero-labels $BUS all
$NDCTL enable-region $BUS all

# if the kernel accounting is correct we should be able to create two
# pmem and two blk namespaces on nfit_test.0
rc=1
$NDCTL create-namespace $BUS -t pmem
$NDCTL create-namespace $BUS -t pmem
$NDCTL create-namespace $BUS -t blk -m raw
$NDCTL create-namespace $BUS -t blk -m raw

# clearnup and exit
$NDCTL disable-region $BUS all
$NDCTL disable-region $BUS1 all
modprobe -r nfit_test

exit 0
