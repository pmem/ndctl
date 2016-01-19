#!/bin/bash -x
DEV=""
NDCTL="./ndctl"
BUS="-b nfit_test.0"
json2var="s/[{}\",]//g; s/:/=/g"
SECTOR_SIZE="4096"

set -e

# setup (reset nfit_test dimms)
modprobe nfit_test
$NDCTL disable-region $BUS all
$NDCTL zero-labels $BUS all
$NDCTL enable-region $BUS all

# create pmem
dev="x"
json=$($NDCTL create-namespace -t pmem -m raw)
eval $(echo $json | sed -e "$json2var")
[ $dev = "x" ] && echo "fail: $LINENO" && exit 1
[ $mode != "raw" ] && echo "fail: $LINENO" &&  exit 1

# convert pmem to memory mode
json=$($NDCTL create-namespace -m memory -f -e $dev)
eval $(echo $json | sed -e "$json2var")
[ $mode != "memory" ] && echo "fail: $LINENO" &&  exit 1

# convert pmem to sector mode
json=$($NDCTL create-namespace -m sector -l $SECTOR_SIZE -f -e $dev)
eval $(echo $json | sed -e "$json2var")
[ $sector_size != $SECTOR_SIZE ] && echo "fail: $LINENO" &&  exit 1
[ $mode != "sector" ] && echo "fail: $LINENO" &&  exit 1

# create blk
dev="x"
json=$($NDCTL create-namespace -t blk -m raw)
eval $(echo $json | sed -e "$json2var")
[ $dev = "x" ] && echo "fail: $LINENO" && exit 1
[ $mode != "raw" ] && echo "fail: $LINENO" &&  exit 1

# convert blk to sector mode
json=$($NDCTL create-namespace -m sector -l $SECTOR_SIZE -f -e $dev)
eval $(echo $json | sed -e "$json2var")
[ $sector_size != $SECTOR_SIZE ] && echo "fail: $LINENO" &&  exit 1
[ $mode != "sector" ] && echo "fail: $LINENO" &&  exit 1

exit 0
