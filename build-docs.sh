#!/bin/bash -ex

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

build_tree()
{
	rm -rf build
	meson setup build
	meson configure -Dasciidoctor=enabled build
	meson compile -C build #Documentation #/ndctl #/attrs.adoc
	build_ver=$(git describe HEAD --tags)
	build_tag=${build_type}-${build_ver#v}
}

man_to_md()
{
	local file="$1"

	test -f "$file" || return 1
	fname=$(basename $file)
	[[ "$fname" == ndctl*.txt ]] || [[ "$fname" == daxctl*.txt ]] || \
		[[ "$fname" == cxl*.txt ]] || [[ "$fname" == libcxl*.txt ]] || return 0
	cfg=$(dirname $file)/asciidoc.conf
	mkdir -p md
	out="md/${fname/%.txt/.md}"
	cat <<- EOF > $out
		---
		layout: page
		---

	EOF
	# sed replacements key:
	# 1. replace ndctl-<>1 with url to that page in markdown format
	# 2. replace 'ndctl-<>\[1\]' special case in daxctl*.md and cxl pages
	# 3. remove 'linkdaxctl:'
	# 4. remove 'linkcxl:'
	# 5. remove 'linklibcxl:'
	# 6. replace daxctl-<>1 with url to that page in markdown format
	# 7. replace cxl-.*1 with url to that page in markdown format
	# 8. replace cxl_.*3 with url to that page in markdown format
	# 9. enclose the option names in literal `` blocks
	# 10. same as 2, but for non option arguments (e.g. <dimm>)
	asciidoctor -b docbook5 \
			-I $ndir/Documentation/ndctl \
			-I $ndir/Documentation/daxctl \
			-I $ndir/Documentation/cxl \
			-I $ndir/Documentation/cxl/lib \
			-I $bdir/Documentation/ndctl \
			-I $bdir/Documentation/daxctl \
			-I $bdir/Documentation/cxl \
			-I $bdir/Documentation/cxl/lib \
			-r asciidoctor-extensions \
			-amansource=ndctl \
			-amanmanual="ndctl Manual" \
			-andctl_version=$build_ver \
			-o- $file | \
		pandoc -f docbook -t gfm | \
		sed -e 's/\(ndctl-[a-z-]*\)1/[\1](\1)/g' | \
		sed -e 's/\(ndctl[a-z-]*\)\\\[1\\\]/[\1](\1)/g' | \
		sed -e "s/linkdaxctl://g" | \
		sed -e "s/linkcxl://g" | \
		sed -e "s/linklibcxl://g" | \
		sed -e 's/\(daxctl[a-z-]*\)\\\[1\\\]/[\1](\1)/g' | \
		sed -e 's/\(cxl[a-z-]*\)\\\[1\\\]/[\1](\1)/g' | \
		sed -e 's/\(cxl_[a-z_]*\)\\\[3\\\]/[\1](\1)/g' | \
		sed -e 's/^\([-]\{1,2\}.*\)  $/`\1`  /g' | \
		sed -e 's/^&lt;/`</g' -e 's/&gt;  $/>`  /g' \
			>> $out

	# NOTE: pandoc now warns that markdown_github is deprecated, and
	# we must use gfm instead, but gfm's rendering of the 'options'
	# sections will need further tweaking. It also renders sed keys 2
	# and 3 useless. Keep markdown_gfm for now until our hand is forced
}

build_index()
{
	name="$1"

	pushd "$name" > /dev/null

	release_url="$ndc_url/releases/tag/$build_ver"
	tree_url="$ndc_url/tree/$build_ver"

	cat <<- EOF > index.md
		---
		title: $name
		layout: home
		---
		[View the Project on GitHub]($ndc_url)  
		**Generated from [$build_tag]($release_url) [[tree]]($tree_url)**  

		---

		* [$name]($name)
	EOF

	for file in ./"$name"-*.md; do
		test -f "$file" || continue
		file=$(basename $file)
		printf "* [${file%%.*}](${file%%.*})\n" >> index.md
	done
	if [[ $name == "lib"* ]]; then
		lib_prefix=${name##lib}
		for file in ./"$lib_prefix"_*.md; do
			test -f "$file" || continue
			file=$(basename $file)
			printf "* [${file%%.*}](${file%%.*})\n" >> index.md
		done
	fi
	popd > /dev/null
}

generate_man_subdirs()
{
	local name="$1"

	pushd "$wdir" >/dev/null

	if [[ $name == "lib"* ]]; then
		path="$ndir/Documentation/${name##lib}/lib"
	else
		path="$ndir/Documentation/$name"
	fi

	for file in $path/*.txt; do
		test -f "$file" || continue
		man_to_md $file
	done
	popd >/dev/null

	test -d "$name" && rm -rf "$name"
	mkdir -p "$name"
	mv $wdir/md/*.md $name

	build_index $name
}

wdir="docs-build"
ndir="ndctl"
bdir="$ndir/build"
tree="$wdir/$ndir"
ndc_url="https://github.com/pmem/ndctl"
deftree="$ndc_url.git"

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
test -d "Documentation/cxl" || badtree

build_tree
popd >/dev/null
popd >/dev/null

generate_man_subdirs ndctl
generate_man_subdirs daxctl
generate_man_subdirs cxl
generate_man_subdirs libcxl

rm -rf "$wdir"

cat <<- EOF

	Documentation build completed.
	The .md files that are used to generate the site have been updated.

	To preview the full site locally, run:
	  bundle exec jekyll serve

	To publish the updated site, commit the changes and push to the
	gh-pages branch:
	  git push origin gh-pages

EOF
