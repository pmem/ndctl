#!/bin/bash
set -e

NAME=ndctl
REFDIR="$HOME/git/ndctl"  # for faster cloning, if available
UPSTREAM=$REFDIR #TODO update once we have a public upstream
OUTDIR=$HOME/rpmbuild/SOURCES

[ -n "$1" ] && HEAD="$1" || HEAD="HEAD"

WORKDIR="$(mktemp -d --tmpdir "$NAME.XXXXXXXXXX")"
trap 'rm -rf $WORKDIR' exit

[ -d "$REFDIR" ] && REFERENCE="--reference $REFDIR"
git clone $REFERENCE "$UPSTREAM" "$WORKDIR"

VERSION=$(./git-version)
DIRNAME="ndctl-${VERSION}"
git archive --remote="$WORKDIR" --format=tar --prefix="$DIRNAME/" HEAD | gzip > $OUTDIR/"v${VERSION}.tar.gz"

echo "Written $OUTDIR/v${VERSION}.tar.gz"
