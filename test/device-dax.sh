#!/bin/bash
NDCTL="../ndctl/ndctl"
json2var="s/[{}\",]//g; s/:/=/g; s/\]//g"
rc=77

err() {
	rc=$?
	echo "device-dax: failed at line $1"
	exit $rc
}

eval $(uname -r | awk -F. '{print "maj="$1 ";" "min="$2}')
if [ $maj -lt 4 ]; then
	echo "kernel $maj.$min lacks device-dax"
	exit $rc
elif [ $maj -eq 4 -a $min -lt 7 ]; then
	echo "kernel $maj.$min lacks device-dax"
	exit $rc
fi

set -e -x
trap 'err $LINENO' ERR

dev=$(./dax-dev)
json=$($NDCTL list -N -n $dev)
eval $(echo $json | sed -e "$json2var")

# setup a device-dax configuration
json=$($NDCTL create-namespace -v -m dax -M dev -f -e $dev)
eval $(echo $json | sed -e "$json2var")
[ $mode != "dax" ] && echo "fail: $LINENO" &&  exit 1

./device-dax /dev/$chardev

# revert namespace to raw mode
json=$($NDCTL create-namespace -v -m raw -f -e $dev)
eval $(echo $json | sed -e "$json2var")
[ $mode != "memory" ] && echo "fail: $LINENO" &&  exit 1

exit 0
