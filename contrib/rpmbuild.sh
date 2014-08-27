#!/bin/bash
$(dirname $0)/make-git-snapshot.sh
rpmbuild -bb $(dirname $0)/ndctl.spec
