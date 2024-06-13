#!/bin/bash
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024 Intel Corporation. All rights reserved.



# Save original stdout/stderr so we can restore them later
exec 3>&1 4>&2

# Redirect all output from here to the debug log
exec > cxl-translate.log 2>&1
export PS4='+ ${BASH_SOURCE}:${LINENO}:${FUNCNAME[0]}: '
set -ex

# err
# $1: line number which error detected
# $2: cleanup function (optional)
#
err()
{
	local lineno=$1
	set +x
	exec 1>&3 2>&4  # restore stdout/stderr
	echo "translate: FAIL at line $lineno (See cxl-translate.log)"
	exit 1
}

trap 'err $LINENO' ERR

rc=1

TEST="./translate"
MODULO=0
XOR=1

# Test against 'Sample Sets' and 'XOR Tables'
#
# Sample Set's have a pattern and the expected HPAs have been verified
# although maybe not published. They verify Modulo and XOR translations.
#
# XOR Table's are extracted from the CXL Driver Writers Guide [1].
# Although the XOR Tables do not include an explicit check of the Modulo
# translation result, a Modulo calculation is always the first step in
# any XOR calculation. ie. if Modulo fails so does XOR.
#
# [1] https://www.intel.com/content/www/us/en/content-details/643805/cxl-memory-device-sw-guide.html


# Sample Sets
#
# params_#: dpa, region eiw, region eig, host bridge eiw
# expect_[modulo|xor]_#: expected hpa for each position in the region
# 	interleave set for the modulo|xor math.
#
# Feeds the parameters with an expected hpa for each position in the
# region interleave to TEST. The test performs the same calculations
# as the CXL Driver and returns success if its calculation matches
# the expected hpa.

# 1+1+1+1
# 4 way region interleave using 4 host bridges
declare -A Sample_4R_4H=(
	["params_0"]="0 2 0 2"
	["expect_modulo_0"]="0 256 512 768"
	["expect_xor_0"]="0 256 512 768"
	["params_1"]="256 2 0 2"
	["expect_modulo_1"]="1024 1280 1536 1792"
	["expect_xor_1"]="1024 1280 1536 1792"
	["params_2"]="2048 2 0 2"
	["expect_modulo_2"]="8192 8448 8704 8960"
	["expect_xor_2"]="8192 8448 8704 8960"
)

# 1+1+1+1+1+1+1+1+1+1+1+1
# 12 way region interleave using 12 host bridges
declare -A Sample_12R_12H=(
	["params_0"]="0 10 0 10"
	["expect_modulo_0"]="0 256 512 768 1024 1280 1536 1792 2048 2304 2560 2816"
	["expect_xor_0"]="0 256 512 768 1024 1280 1536 1792 2304 2048 2816 2560"
	["params_1"]="512 10 0 10"
	["expect_modulo_1"]="6144 6400 6656 6912 7168 7424 7680 7936 8192 8448 8704 8960"
	["expect_xor_1"]="6912 6656 6400 6144 7936 7680 7424 7168 8192 8448 8704 8960"
)

decode_r_eiw()
{
	case $1 in
		0) echo 1 ;;
		1) echo 2 ;;
		2) echo 4 ;;
		3) echo 8 ;;
		4) echo 16 ;;
		8) echo 3 ;;
		9) echo 6 ;;
		10) echo 12 ;;
		*) echo "Invalid r_eiw value: $1" ; err "$LINENO" ;;
	esac
}

test_sample_set()
{
	local -n sample_set=$1
	local sample_count=$((${#sample_set[@]} / 3))

	for i in $(seq 0 $((sample_count - 1))); do
		# Split the parameters and expected hpa values
		IFS=' ' read -r dpa r_eiw r_eig hb_eiw <<< "${sample_set["params_$i"]}"
		IFS=' ' read -r -a expect_modulo_values <<< "${sample_set["expect_modulo_$i"]}"
		IFS=' ' read -r -a expect_xor_values <<< "${sample_set["expect_xor_$i"]}"

		ways=$(decode_r_eiw "$r_eiw")
		for ((pos = 0; pos < ways; pos++)); do
			expect_hpa_modulo=${expect_modulo_values[$pos]}
			expect_hpa_xor=${expect_xor_values[$pos]}

			"$TEST" "$dpa" "$pos" "$r_eiw" "$r_eig" "$hb_eiw" $MODULO "$expect_hpa_modulo" || {
				err "$LINENO"
			}
# Don't Skip the XOR
			"$TEST" "$dpa" "$pos" "$r_eiw" "$r_eig" "$hb_eiw" $XOR "$expect_hpa_xor" || {
				err "$LINENO"
			}
		done
	done
}

# XOR Tables
#
# The tables that follow are the XOR translation examples in the
# CXL Driver Writers Guide Sections 2.13.24.1 and 25.1
#
# Format: "dpa pos r_eiw r_eig h_eiw xor_hpa:

# 1+1+1+1
# 4 way region interleave using 4 host bridges
XOR_Table_4R_4H=(
	"248   0 2 0 2 248"
	"16    1 2 0 2 272"
	"16    2 2 0 2 528"
	"32    3 2 0 2 800"
	"288   0 2 0 2 1056"
	"288   1 2 0 2 1312"
	"288   2 2 0 2 1568"
	"288   3 2 0 2 1824"
	"544   1 2 0 2 2080"
	"544   0 2 0 2 2336"
	"544   3 2 0 2 2592"
	"1040  2 2 0 2 4112"
	"1568  3 2 0 2 6176"
	"32784 1 2 0 2 131088"
	"65552 2 2 0 2 262160"
	"98336 3 2 0 2 393248"
	"98328 2 2 0 2 393496"
	"98352 2 2 0 2 393520"
	"443953523 0 2 0 2 1775813747"
)

# 2+2+2+2
# 8 way region interleave using 4 host bridges
#
#      dpa pos r_eiw r_eig h_eiw xor_hpa
#
XOR_Table_8R_4H=(
	"248       0 3 0 2 248"
	"16        1 3 0 2 272"
	"16        2 3 0 2 528"
	"32        3 3 0 2 800"
	"272       1 3 0 2 2064"
	"528       2 3 0 2 4112"
	"800       3 3 0 2 6176"
	"16400     1 3 0 2 131088"
	"32784     2 3 0 2 262160"
	"49184     3 3 0 2 393248"
	"49176     2 3 0 2 393496"
	"49200     2 3 0 2 393520"
	"116520373 3 3 0 2 932162229"
	"244690459 5 3 0 2 1957525275"
	"292862215 5 3 0 2 2342899463"
	"30721158  4 3 0 2 245769350"
	"246386959 4 3 0 2 1971096847"
	"72701249  5 3 0 2 581610561"
	"529382429 5 3 0 2 4235060509"
	"191132300 2 3 0 2 1529057420"
	"18589081  1 3 0 2 148712089"
	"344295715 7 3 0 2 2754367011"
)

# 1+1+1+1+1+1+1+1+1+1+1+1
# 12 way region interleave using 12 host bridges
XOR_Table_12R_12H=(
	"224 0 10 0 10 224"
	"16  1 10 0 10 272"
	"16  2 10 0 10 528"
	"32  3 10 0 10 800"
	"32  4 10 0 10 1056"
	"32  5 10 0 10 1312"
	"32  6 10 0 10 1568"
	"32  7 10 0 10 1824"
	"32  9 10 0 10 2080"
	"32  8 10 0 10 2336"
	"32 11 10 0 10 2592"
	"32 10 10 0 10 2848"
	"288 0 10 0 10 3360"
	"299017087 7 10 0 10 3588205439"
	"329210435 0 10 0 10 3950524995"
	"151050637 11 10 0 10 1812608653"
	"145169214  2 10 0 10 1742030654"
	"328998732 10 10 0 10 3947985996"
	"159252439  3 10 0 10 1911027415"
	"342098916  5 10 0 10 4105186020"
	"97970344   8 10 0 10 1175645096"
	"214995572  8 10 0 10 2579948404"
	"101289661  7 10 0 10 1215475645"
	"40424079   7 10 0 10 485088911"
	"231458716  7 10 0 10 2777503900"
)

test_xor_table()
{
	local -n samples=$1

	for sample in "${samples[@]}"; do
		IFS=' ' read -r dpa pos r_eiw r_eig hb_eiw xor_hpa <<< "$sample"

		"$TEST" "$dpa" "$pos" "$r_eiw" "$r_eig" "$hb_eiw" $XOR "$xor_hpa" || {
			err "$LINENO"
		}
	done
}

# Process Samples
test_sample_set Sample_4R_4H
test_sample_set Sample_12R_12H
test_xor_table XOR_Table_4R_4H
test_xor_table XOR_Table_8R_4H
test_xor_table XOR_Table_12R_12H

set +x
exec 1>&3 2>&4
echo "All samples processed successfully. (See cxl-translate.log)"
