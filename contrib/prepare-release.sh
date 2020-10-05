#!/bin/bash -e

# Arguments:
#  fix - fixup release instead of a full release
#  ignore_rev - ignore the check for _REVISION in libtool versioning checks

# Notes:
#  - Checkout to the appropriate branch beforehand
#     master - for major release
#     ndctl-xx.y - for fixup release
#    This is important for generating the shortlog
#  - Add a temporary commit that updates the libtool versions as needed.
#    This will later become the release commit. Use --amend to add in the
#    git-version update and the message body.

# Pre-reqs:
#  - libabigail  (for abipkgdiff)
#  - fedpkg (for mock build)

# TODO
#  - auto generate a release commit/tag message template
#  - determine the most recent kernel release and add it to the above
#  - perform documentation update for pmem.io/ndctl

cleanup()
{
	rm -rf release
	mkdir release/
}

err()
{
	echo "$1"
	exit 1
}

parse_args()
{
	local args="$*"
	grep -q "fix" <<< "$args" && rel_fix="1" || rel_fix=""
	grep -q "ignore_rev" <<< "$args" && ignore_rev="1" || ignore_rev=""
}

check_branch()
{
	local cur=$(git rev-parse --abbrev-ref HEAD 2>/dev/null)
	if [ -n "$rel_fix" ]; then
		# fixup release, expect ndctl-xx.y branch
		if ! grep -Eq "^ndctl.[0-9]+\.y$" <<< "$cur"; then
			err "expected an ndctl-xx.y branch for fixup release"
		fi
	else
		# major release, expect master branch
		if ! grep -Eq "^master$" <<< "$cur"; then
			err "expected master branch for a major release"
		fi
	fi
	if ! git diff-index --quiet HEAD --; then
		err "$cur has uncommitted/unstaged changes"
	fi
}

last_maj()
{
	git tag  | sort -V | grep -E "v[0-9]+$" | tail -1
}

last_fix()
{
	local base="$1"
	git tag  | sort -V | grep -E "$base\.?[0-9]*$" | tail -1
}

next_maj()
{
	local last="$1"
	local num=${last#v}

	newnum="$((num + 1))"
	echo "v$newnum"
}

next_fix()
{
	local last="$1"
	local num=${last##*.}
	local base=${last%%.*}

	newnum=$((num + 1))
	echo "$base.$newnum"
}

gen_lists()
{
	local range="$1"

	git shortlog --no-merges "$range" > release/shortlog
	git log --no-merges --pretty=format:"%s" "$range" > release/commits
	c_count=$(git log --pretty=format:"%s" "$range" | wc -l)
}

# Check libtool versions in Makefile.am.in
# $1: lib name (currently libndctl or libdaxctl)
check_libtool_vers()
{
	local lib="$1"
	local lib_u="${lib^^}"
	local libdir="${lib##lib}/lib/"
	local symfile="${libdir}/${lib}.sym"
	local last_cur=$(git show $last_ref:Makefile.am.in | grep -E "^${lib_u}_CURRENT" | cut -d'=' -f2)
	local last_rev=$(git show $last_ref:Makefile.am.in | grep -E "^${lib_u}_REVISION" | cut -d'=' -f2)
	local last_age=$(git show $last_ref:Makefile.am.in | grep -E "^${lib_u}_AGE" | cut -d'=' -f2)
	local last_soname=$((last_cur - last_age))
	local next_cur=$(git show HEAD:Makefile.am.in | grep -E "^${lib_u}_CURRENT" | cut -d'=' -f2)
	local next_rev=$(git show HEAD:Makefile.am.in | grep -E "^${lib_u}_REVISION" | cut -d'=' -f2)
	local next_age=$(git show HEAD:Makefile.am.in | grep -E "^${lib_u}_AGE" | cut -d'=' -f2)
	local next_soname=$((next_cur - next_age))
	local soname_diff=$((next_soname - last_soname))

	# generally libtool versions either reset to zero or increase only by one
	# _CURRENT monotonically increases (by one)
	if [ "$((next_cur - last_cur))" -gt 1 ]; then
		err "${lib_u}_CURRENT can increase at most by 1"
	fi
	if [ "$next_rev" -ne 0 ]; then
		if [ "$((next_rev - last_rev))" -gt 1 ]; then
			err "${lib_u}_REVISION can increase at most by 1"
		fi
	fi
	if [ "$next_age" -ne 0 ]; then
		if [ "$((next_age - last_age))" -gt 1 ]; then
			err "${lib_u}_AGE can increase at most by 1"
		fi
	fi

	# test for soname change
	if [ "$soname_diff" -ne 0 ]; then
		err "${lib}: expected soname to stay unchanged"
	fi

	# tests based on whether symfile changed
	# compatibility breaking changes are left for libabigail to detect
	test -s "$symfile" || err "$symfile: not found"
	if [ -n "$(git diff --name-only $last_ref..HEAD $symfile)" ]; then
		# symfile has changed, cur and age should increase
		if [ "$((next_cur - last_cur))" -ne 1 ]; then
			err "based on $symfile, ${lib_u}_CURRENT should've increased by 1"
		fi
		if [ "$((next_age - last_age))" -ne 1 ]; then
			err "based on $symfile, ${lib_u}_AGE should've increased by 1"
		fi
	else
		# no changes to symfile, revision should've increased if source changed
		if [ -n "$ignore_rev" ]; then
			: # skip
		elif [ -n "$(git diff --name-only $last_ref..HEAD $libdir/)" ]; then
			if [ "$((next_rev - last_rev))" -ne 1 ]; then
				err "based on $symfile, ${lib_u}_REVISION should've increased by 1"
			fi
		fi
	fi
}


# main
cleanup
parse_args "$*"
check_branch
[ -e "COPYING" ] || err "Run from the top level of an ndctl tree"

last_maj=$(last_maj)
test -n "$last_maj" || err "Unable to determine last release"

last_fix=$(last_fix $last_maj)
test -n "$last_fix" || err "Unable to determine last fixup tag for $last_maj"

next_maj=$(next_maj "$last_maj")
next_fix=$(next_fix "$last_fix")
[ -n "$rel_fix" ] && last_ref="$last_fix" || last_ref="$last_maj"
[ -n "$rel_fix" ] && next_ref="$next_fix" || next_ref="$next_maj"

check_libtool_vers "libndctl"
check_libtool_vers "libdaxctl"

# HEAD~1 because HEAD would be the release commit
gen_lists ${last_ref}..HEAD~1

# For ABI diff purposes, use the latest fixes tag
contrib/do_abidiff ${last_fix}..HEAD

# once everything passes, update the git-version
sed -i -e "s/DEF_VER=[0-9]\+.*/DEF_VER=${next_ref#v}/" git-version

echo "Ready to release ndctl-$next_ref with $c_count new commits."
echo "Add git-version to the top commit to get the updated version."
echo "Use release/commits and release/shortlog to compose the release message"
echo "The release commit typically contains the Makefile.am.in libtool version"
echo "update, and the git-version update."
echo "Finally, ensure the release commit as well as the tag are PGP signed."
