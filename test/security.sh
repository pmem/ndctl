#!/bin/bash -Ex
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2018-2020 Intel Corporation. All rights reserved.

rc=77
dev=""
id=""
keypath="/etc/ndctl/keys"
masterkey="nvdimm-master"
masterpath="$keypath/$masterkey.blob"
backup_key=0
backup_handle=0

. $(dirname $0)/common

trap 'err $LINENO' ERR

setup()
{
	$NDCTL disable-region -b "$TEST_BUS" all
}

setup_keys()
{
	if [ ! -d "$keypath" ]; then
		mkdir -p "$keypath"
	fi

	if [ -f "$masterpath" ]; then
		mv "$masterpath" "$masterpath.bak"
		backup_key=1
	fi
	if [ -f "$keypath/tpm.handle" ]; then
		mv "$keypath/tpm.handle" "$keypath/tpm.handle.bak"
		backup_handle=1
	fi

	# Make sure there is a session and a user keyring linked into it
	keyctl new_session
	keyctl link @u @s
	dd if=/dev/urandom bs=1 count=32 2>/dev/null | keyctl padd user "$masterkey" @u
	keyctl pipe "$(keyctl search @u user $masterkey)" > "$masterpath"
}

test_cleanup()
{
	if keyctl search @u encrypted nvdimm:"$id"; then
		keyctl unlink "$(keyctl search @u encrypted nvdimm:"$id")"
	fi

	if keyctl search @u user "$masterkey"; then
		keyctl unlink "$(keyctl search @u user "$masterkey")"
	fi

	if [ -f "$keypath"/nvdimm_"$id"_"$(hostname)".blob ]; then
		rm -f "$keypath"/nvdimm_"$id"_"$(hostname)".blob
	fi
}

post_cleanup()
{
	if [ -f $masterpath ]; then
		rm -f "$masterpath"
	fi
	if [ "$backup_key" -eq 1 ]; then
		mv "$masterpath.bak" "$masterpath"
	fi
	if [ "$backup_handle" -eq 1 ]; then
		mv "$keypath/tpm.handle.bak" "$keypath/tpm.handle"
	fi
}

get_frozen_state()
{
	$NDCTL list -i -b "$TEST_BUS" -d "$dev" | jq -r .[].dimms[0].security_frozen
}

get_security_state()
{
	$NDCTL list -i -b "$TEST_BUS" -d "$dev" | jq -r .[].dimms[0].security
}

setup_passphrase()
{
	$NDCTL setup-passphrase "$dev" -k user:"$masterkey"
	sstate="$(get_security_state)"
	if [ "$sstate" != "unlocked" ]; then
		echo "Incorrect security state: $sstate expected: unlocked"
		err "$LINENO"
	fi
}

remove_passphrase()
{
	$NDCTL remove-passphrase "$dev"
	sstate="$(get_security_state)"
	if [ "$sstate" != "disabled" ]; then
		echo "Incorrect security state: $sstate expected: disabled"
		err "$LINENO"
	fi
}

erase_security()
{
	$NDCTL sanitize-dimm -c "$dev"
	sstate="$(get_security_state)"
	if [ "$sstate" != "disabled" ]; then
		echo "Incorrect security state: $sstate expected: disabled"
		err "$LINENO"
	fi
}

update_security()
{
	$NDCTL update-passphrase "$dev"
	sstate="$(get_security_state)"
	if [ "$sstate" != "unlocked" ]; then
		echo "Incorrect security state: $sstate expected: unlocked"
		err "$LINENO"
	fi
}

freeze_security()
{
	$NDCTL freeze-security "$dev"
}

test_1_security_setup_and_remove()
{
	setup_passphrase
	remove_passphrase
}

test_2_security_setup_and_update()
{
	setup_passphrase
	update_security
	remove_passphrase
}

test_3_security_setup_and_erase()
{
	setup_passphrase
	erase_security
}

test_4_security_unlock()
{
	setup_passphrase
	lock_dimm
	$NDCTL enable-dimm "$dev"
	sstate="$(get_security_state)"
	if [ "$sstate" != "unlocked" ]; then
		echo "Incorrect security state: $sstate expected: unlocked"
		err "$LINENO"
	fi
	$NDCTL disable-region -b "$TEST_BUS" all
	remove_passphrase
}

# This should always be the last nvdimm security test.
# with security frozen, nfit_test must be removed and is no longer usable
test_5_security_freeze()
{
	setup_passphrase
	freeze_security
	sstate="$(get_security_state)"
	fstate="$(get_frozen_state)"
	if [ "$fstate" != "true" ]; then
		echo "Incorrect security state: expected: frozen"
		err "$LINENO"
	fi
	$NDCTL remove-passphrase "$dev" && { echo "remove succeed after frozen"; }
	sstate2="$(get_security_state)"
	if [ "$sstate" != "$sstate2" ]; then
		echo "Incorrect security state: $sstate2 expected: $sstate"
		err "$LINENO"
	fi
}

test_6_load_keys()
{
	if keyctl search @u encrypted nvdimm:"$id"; then
		keyctl unlink "$(keyctl search @u encrypted nvdimm:"$id")"
	fi

	if keyctl search @u user "$masterkey"; then
		keyctl unlink "$(keyctl search @u user "$masterkey")"
	fi

	$NDCTL load-keys

	if keyctl search @u user "$masterkey"; then
		echo "master key loaded"
	else
		echo "master key failed to loaded"
		err "$LINENO"
	fi

	if keyctl search @u encrypted nvdimm:"$id"; then
		echo "dimm key loaded"
	else
		echo "dimm key failed to load"
		err "$LINENO"
	fi
}

if [ "$1" = "nfit" ]; then
	. $(dirname $0)/nfit-security
	TEST_BUS="$NFIT_TEST_BUS0"
	check_min_kver "5.0" || do_skip "may lack security handling"
	KMOD_TEST="nfit_test"
elif [ "$1" = "cxl" ]; then
	. $(dirname $0)/cxl-security
	TEST_BUS="$CXL_TEST_BUS"
	check_min_kver "6.2" || do_skip "may lack security handling"
	KMOD_TEST="cxl_test"
else
	do_skip "Missing input parameters"
fi

uid="$(keyctl show | grep -Eo "_uid.[0-9]+" | head -1 | cut -d. -f2-)"
if [ "$uid" -ne 0 ]; then
	do_skip "run as root or with a sudo login shell for test to work"
fi

modprobe "$KMOD_TEST"
$CXL list
setup
check_prereq "keyctl"
rc=1
detect
test_cleanup
setup_keys
echo "Test 1, security setup and remove"
test_1_security_setup_and_remove
echo "Test 2, security setup, update, and remove"
test_2_security_setup_and_update
echo "Test 3, security setup and erase"
test_3_security_setup_and_erase
echo "Test 4, unlock dimm"
test_4_security_unlock

# Freeze should always be the last nvdimm security test because it locks
# security state and require nfit_test module unload. However, this does
# not impact any key management testing via libkeyctl.
echo "Test 5, freeze security"
test_5_security_freeze

# Load-keys is independent of actual nvdimm security and is part of key
# management testing.
echo "Test 6, test load-keys"
test_6_load_keys

test_cleanup
post_cleanup
if [ "$1" = "nfit" ]; then
	_cleanup
elif [ "$1" = "cxl" ]; then
	_cxl_cleanup
fi

exit 0
