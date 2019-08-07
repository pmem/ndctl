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
	./configure --enable-asciidoctor
	make -C Documentation/ndctl attrs.adoc
	make -C Documentation/ndctl asciidoctor-extensions.rb
	make -C Documentation/daxctl asciidoctor-extensions.rb
	build_ver=$(git describe HEAD --tags)
	build_tag=${build_type}-${build_ver#v}
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
	# 2. replace 'ndctl-<>\[1\]' special case in daxctl*.md pages
	# 3. remove 'linkdaxctl:'
	# 4. replace daxctl-<>1 with url to that page in markdown format
	# 5. enclose the option names in literal `` blocks
	# 6. same as 2, but for non option arguments (e.g. <dimm>)
	asciidoctor -b docbook5 \
			-I $ndir/Documentation/ndctl \
			-I $ndir/Documentation/daxctl \
			-r asciidoctor-extensions \
			-amansource=ndctl \
			-amanmanual="ndctl Manual" \
			-andctl_version=$build_ver \
			-o- $file | \
		pandoc -f docbook -t markdown_github | \
		sed -e 's/\(ndctl-[a-z-]*\)1/[\1](\1.md)/g' | \
		sed -e 's/\(ndctl-[a-z-]*\)\\\[1\\\]/[\1](\1.md)/g' | \
		sed -e "s/linkdaxctl://g" | \
		sed -e 's/\(daxctl[a-z-]*\)\\\[1\\\]/[\1](\1.md)/g' | \
		sed -e 's/^\([-]\{1,2\}.*\)  $/`\1`  /g' | \
		sed -e 's/^&lt;/`</g' -e 's/&gt;  $/>`  /g' \
			>> $out

	# NOTE: pandoc now warns that markdown_github is deprecated, and
	# we must use gfm instead, but gfm's rendering of the 'options'
	# sections will need further tweaking. It also renders sed keys 2
	# and 3 useless. Keep markdown_gfm for now until our hand is forced
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
	**Generated from [$build_tag](https://github.com/pmem/ndctl/releases/tag/$build_ver) [[tree]](https://github.com/pmem/ndctl/tree/$build_ver)**  

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
