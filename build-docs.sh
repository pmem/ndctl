#!/bin/bash -e

usage()
{
	echo "./build-docs.sh <branch|tag|path|repo-URL>"
	exit 1
}

badtree()
{
	echo "$tree does not appear to be a valid ndctl source tree"
	exit 1
}

wdir="build"
ndir="ndctl"
tree="$wdir/$ndir"
deftree="https://github.com/pmem/ndctl.git"

test -d "$wdir" && rm -rf "$wdir"
mkdir -p "$wdir"
pushd "$wdir" >/dev/null

loc="$1"
test -z "$loc" && usage

# parse 'loc' and populate $tree
build_type=""
if git ls-remote -q --exit-code "$deftree" "$loc"; then
	git clone -q --branch="$loc" --depth=1 "$deftree" "$ndir"
	build_type="ndctl"
else
	git clone "$loc"
	build_type="local-build"
fi

# make sure tree appears to be valid
pushd "$ndir" >/dev/null
test -d "Documentation/ndctl" || badtree
test -d "Documentation/daxctl" || badtree

# build the asciidoc config
build_ver=""
build_tag=""
build_tree()
{
	./autogen.sh
	./configure
	make -C Documentation/ndctl asciidoc.conf
	make -C Documentation/daxctl asciidoc.conf
	build_ver=$(git describe HEAD --tags)
	build_tag=${build_type}-${build_ver}
}

build_tree >/dev/null
popd >/dev/null

# convert to md
man_to_md()
{
	local file="$1"

	test -f "$file" || return 1
	fname=$(basename $file)
	[[ "$fname" == ndctl*.txt ]] || [[ "$fname" == daxctl*.txt ]] || return 0
	cfg=$(dirname $file)/asciidoc.conf
	out="md/${fname/%.txt/.md}"
	cat <<- EOF > $out
		---
		title: ndctl
		layout: pmdk
		---

	EOF
	# sed replacements key:
	# 1. replace ndctl-<>1 with url to that page in markdown format
	# 2. enclose the option names in literal `` blocks
	# 3. same as 2, but for non option arguments (e.g. <dimm>)
	asciidoc -b docbook -f $cfg --unsafe -o- $file | \
		pandoc -f docbook -t markdown_github | \
		sed -e "s/\(ndctl-[^1]*\)1/[\1](\1.md)/g" | \
		sed -e 's/^\([-]\{1,2\}.*\)  $/`\1`  /g' | \
		sed -e 's/^&lt;/`</g' -e 's/&gt;  $/>`  /g' \
			>> $out
}

mkdir -p md
for file in $ndir/Documentation/ndctl/*.txt; do
	man_to_md $file
done
for file in $ndir/Documentation/daxctl/*.txt; do
	man_to_md $file
done
popd >/dev/null

cp $wdir/md/*.md .

# generate index
rm -f index.md

cat <<- EOF > index.md
	---
	title: ndctl
	layout: pmdk
	---
	[View the Project on GitHub](https://github.com/pmem/ndctl)  
	**Generated from $build_tag**  

	---

	#### ndctl man pages
	* [ndctl](ndctl.md)
EOF

for file in ./ndctl-*.md; do
	file=$(basename $file)
	printf "* [${file%%.*}](${file})\n" >> index.md
done

cat <<- EOF >> index.md

	---
	#### daxctl man pages
	* [daxctl](daxctl.md)
EOF

for file in ./daxctl-*.md; do
	file=$(basename $file)
	printf "* [${file%%.*}](${file})\n" >> index.md
done

cat <<- EOF

	Documentation build completed.
	The .md files that are used to generate the site have been updated.

	To preview the full site locally, run:
	  bundle exec jekyll serve

	To publish the updated site, commit the changes and push to the
	gh-pages branch:
	  git push origin gh-pages

EOF
