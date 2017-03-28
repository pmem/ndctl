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

check_min_kver "4.11" || { echo "kernel $KVER may not provide reliable isetcookie values"; exit $rc; }

set -e
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

len=$($NDCTL list -r 7 -N | jq -r "length")

if [ -z $len ]; then
	rc=1
	echo "failed to find legacy isetcookie namespace"
	exit 1
fi

$NDCTL disable-region $BUS all
$NDCTL disable-region $BUS1 all
modprobe -r nfit_test

exit 0
