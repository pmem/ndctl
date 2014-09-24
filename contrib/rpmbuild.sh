#!/bin/bash
$(dirname $0)/make-git-snapshot.sh
make -C $(dirname $0)
rpmbuild -bb $(dirname $0)/ndctl.spec
