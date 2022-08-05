#!/bin/bash
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2022 Intel Corporation. All rights reserved.

. $(dirname $0)/common

rc=1

set -ex

trap 'err $LINENO' ERR

check_prereq "jq"

modprobe -r cxl_test
modprobe cxl_test
udevadm settle

# THEORY OF OPERATION: Create a x8 interleave across the pmem capacity
# of the 8 endpoints defined by cxl_test, commit the decoders (which
# just stubs out the actual hardware programming aspect, but updates the
# driver state), and then tear it all down again. As with other cxl_test
# tests if the CXL topology in tools/testing/cxl/test/cxl.c ever changes
# then the paired update must be made to this test.

# find the root decoder that spans both test host-bridges and support pmem
decoder=$($CXL list -b cxl_test -D -d root | jq -r ".[] |
	  select(.pmem_capable == true) |
	  select(.nr_targets == 2) |
	  .decoder")

# find the memdevs mapped by that decoder
readarray -t mem < <($CXL list -M -d $decoder | jq -r ".[].memdev")

# ask cxl reserve-dpa to allocate pmem capacity from each of those memdevs
readarray -t endpoint < <($CXL reserve-dpa -t pmem ${mem[*]} -s $((256<<20)) |
			  jq -r ".[] | .decoder.decoder")

# instantiate an empty region
region=$(cat /sys/bus/cxl/devices/$decoder/create_pmem_region)
echo $region > /sys/bus/cxl/devices/$decoder/create_pmem_region
uuidgen > /sys/bus/cxl/devices/$region/uuid

# setup interleave geometry
nr_targets=${#endpoint[@]}
echo $nr_targets > /sys/bus/cxl/devices/$region/interleave_ways
r_ig=$(cat /sys/bus/cxl/devices/$decoder/interleave_granularity)
echo $r_ig > /sys/bus/cxl/devices/$region/interleave_granularity
echo $((nr_targets * (256<<20))) > /sys/bus/cxl/devices/$region/size

# grab the list of memdevs grouped by host-bridge interleave position
port_dev0=$($CXL list -T -d $decoder | jq -r ".[] |
	    .targets | .[] | select(.position == 0) | .target")
port_dev1=$($CXL list -T -d $decoder | jq -r ".[] |
	    .targets | .[] | select(.position == 1) | .target")
readarray -t mem_sort0 < <($CXL list -M -p $port_dev0 | jq -r ".[] | .memdev")
readarray -t mem_sort1 < <($CXL list -M -p $port_dev1 | jq -r ".[] | .memdev")

# TODO: add a cxl list option to list memdevs in valid region provisioning
# order, hardcode for now.
mem_sort=()
mem_sort[0]=${mem_sort0[0]}
mem_sort[1]=${mem_sort1[0]}
mem_sort[2]=${mem_sort0[2]}
mem_sort[3]=${mem_sort1[2]}
mem_sort[4]=${mem_sort0[1]}
mem_sort[5]=${mem_sort1[1]}
mem_sort[6]=${mem_sort0[3]}
mem_sort[7]=${mem_sort1[3]}

# TODO: use this alternative memdev ordering to validate a negative test for
# specifying invalid positions of memdevs
#mem_sort[2]=${mem_sort0[0]}
#mem_sort[1]=${mem_sort1[0]}
#mem_sort[0]=${mem_sort0[2]}
#mem_sort[3]=${mem_sort1[2]}
#mem_sort[4]=${mem_sort0[1]}
#mem_sort[5]=${mem_sort1[1]}
#mem_sort[6]=${mem_sort0[3]}
#mem_sort[7]=${mem_sort1[3]}

# re-generate the list of endpoint decoders in region position programming order
endpoint=()
for i in ${mem_sort[@]}
do
	readarray -O ${#endpoint[@]} -t endpoint < <($CXL list -Di -d endpoint -m $i | jq -r ".[] |
						     select(.mode == \"pmem\") | .decoder")
done

# attach all endpoint decoders to the region
pos=0
for i in ${endpoint[@]}
do
	echo $i > /sys/bus/cxl/devices/$region/target$pos
	pos=$((pos+1))
done
echo "$region added ${#endpoint[@]} targets: ${endpoint[@]}"

# validate all endpoint decoders have the correct setting
region_size=$(cat /sys/bus/cxl/devices/$region/size)
region_base=$(cat /sys/bus/cxl/devices/$region/resource)
for i in ${endpoint[@]}
do
	iw=$(cat /sys/bus/cxl/devices/$i/interleave_ways)
	ig=$(cat /sys/bus/cxl/devices/$i/interleave_granularity)
	[ $iw -ne $nr_targets ] && err "$LINENO: decoder: $i iw: $iw targets: $nr_targets"
	[ $ig -ne $r_ig] && err "$LINENO: decoder: $i ig: $ig root ig: $r_ig"

	sz=$(cat /sys/bus/cxl/devices/$i/size)
	res=$(cat /sys/bus/cxl/devices/$i/start)
	[ $sz -ne $region_size ] && err "$LINENO: decoder: $i sz: $sz region_size: $region_size"
	[ $res -ne $region_base ] && err "$LINENO: decoder: $i base: $res region_base: $region_base"
done

# validate all switch decoders have the correct settings
nr_switches=$((nr_targets/2))
nr_host_bridges=$((nr_switches/2))
nr_switch_decoders=$((nr_switches + nr_host_bridges))

json=$($CXL list -D -r $region -d switch)
readarray -t switch_decoders < <(echo $json | jq -r ".[].decoder")

[ ${#switch_decoders[@]} -ne $nr_switch_decoders ] && err \
"$LINENO: expected $nr_switch_decoders got ${#switch_decoders[@]} switch decoders"

for i in ${switch_decoders[@]}
do
	decoder=$(echo $json | jq -r ".[] | select(.decoder == \"$i\")")
	id=${i#decoder}
	port_id=${id%.*}
	depth=$($CXL list -p $port_id -S | jq -r ".[].depth")
	iw=$(echo $decoder | jq -r ".interleave_ways")
	ig=$(echo $decoder | jq -r ".interleave_granularity")

	[ $iw -ne 2 ] && err "$LINENO: decoder: $i iw: $iw targets: 2"
	[ $ig -ne $((r_ig << depth)) ] && err \
	"$LINENO: decoder: $i ig: $ig switch_ig: $((r_ig << depth))"

	res=$(echo $decoder | jq -r ".resource")
	sz=$(echo $decoder | jq -r ".size")
	[ $sz -ne $region_size ] && err \
	"$LINENO: decoder: $i sz: $sz region_size: $region_size"
	[ $res -ne $region_base ] && err \
	"$LINENO: decoder: $i base: $res region_base: $region_base"
done

# walk up the topology and commit all decoders
echo 1 > /sys/bus/cxl/devices/$region/commit

# walk down the topology and de-commit all decoders
echo 0 > /sys/bus/cxl/devices/$region/commit

# remove endpoints from the region
pos=0
for i in ${endpoint[@]}
do
	echo "" > /sys/bus/cxl/devices/$region/target$pos
	pos=$((pos+1))
done

# release DPA capacity
readarray -t endpoint < <($CXL free-dpa -t pmem ${mem[*]} |
			  jq -r ".[] | .decoder.decoder")
echo "$region released ${#endpoint[@]} targets: ${endpoint[@]}"

# validate no WARN or lockdep report during the run
log=$(journalctl -r -k --since "-$((SECONDS+1))s")
grep -q "Call Trace" <<< $log && err "$LINENO"

modprobe -r cxl_test
