#!/bin/bash
set -e

NAME=ndctl

if [ ! -x ./git-version ]; then
	echo "$0 : ERROR: Must run from top level of git tree"
	exit 1
fi

REFDIR=$PWD

UPSTREAM=$REFDIR #TODO update once we have a public upstream
OUTDIR=$HOME/rpmbuild/SOURCES

if [ ! -d $OUTDIR ]; then
	mkdir -p $OUTDIR
fi

[ -n "$1" ] && HEAD="$1" || HEAD="HEAD"

WORKDIR="$(mktemp -d --tmpdir "$NAME.XXXXXXXXXX")"
trap 'rm -rf $WORKDIR' exit

[ -d "$REFDIR" ] && REFERENCE="--reference $REFDIR"
git clone $REFERENCE "$UPSTREAM" "$WORKDIR"

VERSION=$(./git-version)
DIRNAME="ndctl-${VERSION}"
git archive --remote="$WORKDIR" --format=tar --prefix="$DIRNAME/" HEAD | gzip > $OUTDIR/"ndctl-${VERSION}.tar.gz"

echo "Written $OUTDIR/ndctl-${VERSION}.tar.gz"
