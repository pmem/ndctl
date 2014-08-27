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

pushd "$WORKDIR" > /dev/null
git branch to-archive $HEAD
read COMMIT_SHORTID COMMIT_TITLE <<EOGIT
$(git log to-archive^..to-archive --pretty='format:%h %s')
EOGIT
popd > /dev/null

echo "Making git snapshot using commit: $COMMIT_SHORTID $COMMIT_TITLE"

DIRNAME="$NAME-git$COMMIT_SHORTID"
git archive --remote="$WORKDIR" --format=tar --prefix="$DIRNAME/" to-archive | xz -9 > $OUTDIR/"$DIRNAME.tar.xz"

echo "Written $OUTDIR/$DIRNAME.tar.xz"
