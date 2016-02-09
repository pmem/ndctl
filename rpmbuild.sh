#!/bin/bash
pushd $(dirname $0) >/dev/null
./make-git-snapshot.sh
popd > /dev/null
rpmbuild -ba $(dirname $0)/rhel/ndctl.spec
