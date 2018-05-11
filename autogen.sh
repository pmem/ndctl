#!/bin/sh -e

if [ -f .git/hooks/pre-commit.sample -a ! -f .git/hooks/pre-commit ] ; then
        cp -p .git/hooks/pre-commit.sample .git/hooks/pre-commit && \
        chmod +x .git/hooks/pre-commit && \
        echo "Activated pre-commit hook."
fi

$(dirname $0)/git-version-gen
reconf_args=''
[ -n "$*" ] && reconf_args="$*"
autoreconf --install --symlink $reconf_args

libdir() {
        echo $(cd $1/$(gcc -print-multi-os-directory); pwd)
}

args="--prefix=/usr \
--sysconfdir=/etc \
--libdir=$(libdir /usr/lib)"

echo
echo "----------------------------------------------------------------"
echo "Initialized build system. For a common configuration please run:"
echo "----------------------------------------------------------------"
echo
echo "./configure CFLAGS='-g -O2' $args"
echo
