#!/bin/bash
NDCTL="../ndctl/ndctl"
rc=77

set -e

err() {
	echo "test/device-dax-fio.sh: failed at line $1"
	exit $rc
}

check_min_kver()
{
	local ver="$1"
	: "${KVER:=$(uname -r)}"

	[ -n "$ver" ] || return 1
	[[ "$ver" == "$(echo -e "$ver\n$KVER" | sort -V | head -1)" ]]
}

check_min_kver "4.11" || { echo "kernel $KVER may lack latest device-dax fixes"; exit $rc; }

set -e
trap 'err $LINENO' ERR

if ! fio --enghelp | grep -q "dev-dax"; then
	echo "fio lacks dev-dax engine"
	exit 77
fi

dev=$(./dax-dev)
for align in 4k 2m 1g
do
	json=$($NDCTL create-namespace -m dax -a $align -f -e $dev)
	chardev=$(echo $json | jq -r ". | select(.mode == \"dax\") | .daxregion.devices[0].chardev")
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
	mode=$(echo $json | jq -r ".mode")
	[ $mode != "memory" ] && echo "fail: $LINENO" && exit 1
done

exit 0
