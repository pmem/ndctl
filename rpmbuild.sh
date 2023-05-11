#!/bin/bash

spec=${1:-$(dirname $0)/rhel/ndctl.spec)}

pushd $(dirname $0) >/dev/null
[ ! -d ~/rpmbuild/SOURCES ] && echo "rpmdev tree not found" && exit 1
if ./git-version | grep -q dirty; then
	echo "Uncommitted changes detected, commit or undo them to proceed"
	git status -uno --short
	exit 1
fi
if [ ! -f $spec ]; then
	meson compile -C build rhel/ndctl.spec || exit
	spec=$(dirname $0)/build/rhel/ndctl.spec
fi
./make-git-snapshot.sh
popd > /dev/null
rpmbuild --nocheck -ba $spec
