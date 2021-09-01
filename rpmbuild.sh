#!/bin/bash

spec=${1:-$(dirname $0)/rhel/ndctl.spec)}

pushd $(dirname $0) >/dev/null
[ ! -d ~/rpmbuild/SOURCES ] && echo "rpmdev tree not found" && exit 1
./make-git-snapshot.sh
popd > /dev/null
rpmbuild --nocheck -ba $spec
