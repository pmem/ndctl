## Documentation for ndctl and daxctl

This branch contains the source for the github-pages documentation site.

_Do not edit the markdown files in this branch by hand. They are auto-generated using the build-docs.sh script, and will be overwritten the next time docs are synced with the main project_

#### Update process:

* Run `build-docs.sh` to update the site files
  * `build-docs.sh` takes one argument, which is either a branch/tag name in the main ndctl repo, or a repo location URL or filesystem path
* To preview the full site locally, run:  
  `bundle exec jekyll serve`
* To publish the updated site, commit the changes and push to the gh-pages branch:  
  `git push origin gh-pages`

#### Setting up jekyll for local testing

* The canonical instructions for installing jekyll can be found at [the official site](https://jekyllrb.com/docs/installation/)
* These steps seem to work on a recent Fedora:
  * dnf install ruby ruby-devel rubygems
  * gem install bundle
  * gem install jekyll
  * <checkout gh-pages, cwd to it>
  * bundle install
  * bundle exec jekyll serve

* Other dependencies for build-docs.sh:
  * `dnf install asciidoc pandoc`
  * Any other dependencies required to build *ndctl*
