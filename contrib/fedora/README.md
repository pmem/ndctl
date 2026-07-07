# ndctl Fedora Package Build Scripts

## Overview

These scripts live in `contrib/fedora/` in the upstream ndctl repo and automate
the process of building and publishing a new ndctl release to Fedora.  They are
meant to be run in order, once per release (~quarterly), from your upstream ndctl
clone — not from the dist-git clone.

The general flow is:
1. Download the new upstream sources and compose a new spec file
2. Sanity-check the dist-git workspace before pushing anything
3. Commit and push the spec/sources to dist-git `main`
4. Fire off Koji builds for main and each stable Fedora branch
5. After the builds are green, submit the bodhi updates
6. Clean up temporary files

Scripts 04 and 05 are deliberately split: 04 submits the builds and exits
(Koji runs them in parallel), you watch them complete on the Koji web page,
and then 05 verifies they finished and submits the bodhi updates.  This gives
a natural human checkpoint instead of a script blocking for hours.

---

## One-Time Setup

These scripts need to know where your local dist-git clone lives.  Copy the
example config and fill in your path:

```bash
cp contrib/fedora/config.local.example contrib/fedora/config.local
# edit config.local and set DISTGIT to your dist-git clone path
```

`config.local` is excluded from git and from release tarballs (see
`.gitattributes`).  It is specific to your machine and never committed.

You can verify your setup at any time without doing any real work by running
any script with `--check`:

```bash
./contrib/fedora/01-prep_build --check vNN
./contrib/fedora/04-do_build_branches --check vNN   # also prints branch list
./contrib/fedora/05-do_submit_updates --check vNN   # also prints the NVRs it would verify
```

---

## Two Repos, One Workflow

These scripts operate across two separate git repos:

| Repo | Purpose | Typical location |
|---|---|---|
| **upstream** (this repo) | ndctl source code; where scripts live | `~/src/ndctl` |
| **dist-git** | Fedora packaging repo (fedpkg clone) | `~/fedora-pkgs/ndctl` |

**All scripts are run from the upstream repo root.**  They use `$DISTGIT` (from
`config.local`) to operate on the dist-git clone as needed.

---

## Branch Strategy

Fedora dist-git uses separate branches per release:

- `main` / `rawhide` — the next unreleased Fedora (e.g. fc45).  Builds here
  but no bodhi update — rawhide gets updates automatically.
- `f44`, `f43`, `f42` — current stable Fedora releases.  Builds here AND
  submit bodhi updates so the package reaches users.
- `f41` and older — EOL, do not build.

**Scripts 01-03 always operate on dist-git `main`.**  Script 04 then fans out
to stable branches by **copying the release files (`ndctl.spec`, `.gitignore`,
`sources`) from `main`** and committing them on each branch.  This means the
new version must land on `main` first, before any stable branch builds are
attempted.

The file copy is used instead of `git merge` on purpose: stable branches can
permanently diverge from main (a Fedora mass rebuild landing on a branch
independently, or a past manual fix-up), which makes fast-forward merges
impossible.  The file copy works identically whether or not a branch has
diverged, and is a no-op if the branch already has the files — safe to re-run.

After script 03, the dist-git branch state should look like:
```
main  → vNN    ← just pushed (the new release)
f44   → vNN-1  ← script 04 will copy the files from main and build
f43   → vNN-1  ← script 04 will copy the files from main and build

(fNN branch names and prior versions shown are examples; the set of stable
branches changes as Fedora releases roll forward)
```

---

## Prerequisites

- `kinit <user>@FEDORAPROJECT.ORG` — Kerberos auth (script 01 handles this
  automatically if `~/.fedora.upn` contains your Fedora username)
- Tools installed: `fedpkg`, `koji`, `rpm-build`, `rpmdev-bumpspec`, `wget`,
  `meson`
- dist-git clone on `main` branch before starting

---

## Script Reference

### 01-prep_build \<version\>

**Purpose:** Download upstream sources, extract the spec from the upstream meson
build, and compose a new `ndctl.spec` in dist-git by merging the upstream spec
body with the existing `%changelog` section.  Also uploads the new tarball to
the Fedora lookaside cache.

**Run from:** upstream repo root (any branch — the release tarball is
downloaded from GitHub by tag; your local upstream tree is not used).
The **dist-git** clone must be on `main`.

**Test without building:**
```bash
./contrib/fedora/01-prep_build --check vNN
```

**After this script:** Review the spec in dist-git before proceeding:
```bash
grep -E "^Version|^Release" $DISTGIT/ndctl.spec
head -30 $DISTGIT/ndctl.spec
```
If something looks wrong, restore the original:
```bash
cp $DISTGIT/ndctl.spec.orig $DISTGIT/ndctl.spec
```

---

### 02-do_build_local_checks \<version\>

**Purpose:** Sanity-check that dist-git is in the right state before committing
anything — confirms `ndctl.spec` was modified and `sources`/`.gitignore` were
staged by `fedpkg new-sources`.

**Run from:** upstream repo root

**Test without building:**
```bash
./contrib/fedora/02-do_build_local_checks --check vNN
```

If this script passes, dist-git is ready to commit and push.  If it fails,
something went wrong in script 01 — do not proceed.

---

### 03-do_build_push \<version\>

**Purpose:** Commit the updated spec/sources in dist-git and push to `main`.
This is the single push that makes the new version available for all stable
branches to copy from.

**Run from:** upstream repo root

**Test without building:**
```bash
./contrib/fedora/03-do_build_push --check vNN
```

**Important:** This pushes to dist-git `main` only.  Script 04 handles the
stable branches.  Pushing to `main` first is critical — script 04 copies the
release files *from main*, so if this push somehow lands on a stable branch
instead, the other branches won't get the new version.  See "Script ran on
wrong branch" in Common Problems.

---

### 04-do_build_branches \<version\>

**Purpose:** Run a scratch build as a gate, then fire off real Koji builds for
main (rawhide) and each stable branch.  The builds run **in parallel** in Koji;
the script prints their URLs and exits without waiting.

**Run from:** upstream repo root

**Test without building (also prints branch list):**
```bash
./contrib/fedora/04-do_build_branches --check vNN
```

**Normal usage:**
```bash
./contrib/fedora/04-do_build_branches vNN
```

**Single branch (retry a failed branch; skips scratch and main):**
```bash
br=f44 ./contrib/fedora/04-do_build_branches vNN
```

**Build more stable branches** (check https://endoflife.date/fedora for the
current stable set; 3 is only needed in the transition window when a new
Fedora is out but the oldest release hasn't gone EOL yet):
```bash
num_stable=3 ./contrib/fedora/04-do_build_branches vNN
```

**What it does:**
1. Scratch build on `main` — the gate; this one WAITS for the result.
   Nothing is pushed to any stable branch until it passes.
2. Fires the real `main` (rawhide) build with `--nowait`
3. For each stable branch: copies `ndctl.spec`, `.gitignore`, `sources` from
   `main`, commits, pushes, and fires the build with `--nowait`
4. Prints all Koji task URLs and exits

**Environment variables:**

| Variable | Default | Purpose |
|---|---|---|
| `br` | (unset) | Build only this one branch |
| `num_stable` | `2` | How many stable branches to build |

**After this script:** watch the builds at the printed Koji URLs.  When all
show green (state `complete`), run script 05.

---

### 05-do_submit_updates \<version\>

**Purpose:** Verify the Koji builds completed, then submit the bodhi updates
for the stable branches.  Bodhi is Fedora's update gating system — submitting
moves the build into `updates-testing` where users can install and karma-test
it before it reaches the stable repo.  A Koji build alone does not reach users.

**Run from:** upstream repo root

**Test without submitting (shows the NVRs it would verify):**
```bash
./contrib/fedora/05-do_submit_updates --check vNN
```

**Normal usage (after all builds are green):**
```bash
./contrib/fedora/05-do_submit_updates vNN
```

**What it does:**
1. Checks every expected build (e.g. `ndctl-85-1.fc44`) is `COMPLETE` in Koji.
   If any is not ready, it lists them and exits without submitting anything —
   wait and re-run.
2. Submits a bodhi update for each stable branch.
   Rawhide/main is skipped — bodhi creates its update automatically.

**Bodhi update type:** a version with a dot (e.g. `vNN.M`) is submitted as a
`bugfix`; otherwise `enhancement`.

**Environment variables:**

| Variable | Default | Purpose |
|---|---|---|
| `br` | (unset) | Submit for only this one branch |
| `num_stable` | `2` | How many stable branches (match what 04 used) |
| `bz` | (unset) | Bugzilla number to attach to the updates |
| `rel` | `1` | RPM Release number, if the build wasn't `-1` |

---

### 06-cleanup

**Purpose:** Remove temporary files from both the upstream repo and dist-git.

**What it removes:**
- `ndctl.spec.src` (upstream repo) — extracted upstream spec from script 01
- `$DISTGIT/ndctl.spec.orig` — backup of the previous Fedora spec
- `$DISTGIT/*.src.rpm` — local source RPMs from mock builds
- `$DISTGIT/results_ndctl/` — mock build output
- `$DISTGIT/copr-out/` — COPR build output

`ndctl.spec` itself is NOT removed — it is the live Fedora spec tracked in
dist-git.

---

## Local vs Scratch Builds

Script 02 has `fedpkg mockbuild` and `fedpkg lint` commented out.  Local mock
builds are fragile — they depend on your local environment, mock group
membership, and locally installed Fedora build root configs, all of which are a
pain to maintain for a quarterly workflow.

Instead, script 04 runs a **scratch build** in Koji as its first step.  A
scratch build runs in Fedora's actual build infrastructure using the exact same
environment as a real build.  If it passes, the spec is good.  This is more
reliable than a local mock build and requires zero local setup.

The scratch build is intentionally the first thing script 04 does.  If it
fails, nothing has been pushed to any stable branch yet, so recovery is clean.

---

## Checking Build and Update Status

**Koji builds:**
```
https://koji.fedoraproject.org/koji/packageinfo?packageID=ndctl
```

**Bodhi updates:**
```
https://bodhi.fedoraproject.org/updates/?packages=ndctl
```

---

## Common Problems

### Scratch build failed

The scratch build in script 04 runs before any stable branch work.  If it
fails, the script aborts and nothing has been pushed to any stable branch yet —
recovery is clean.

Common causes that local checks won't catch:
- Missing or incorrect `BuildRequires` in the spec
- Upstream source tarball has a different directory layout than the spec expects
- A patch no longer applies cleanly against the new version
- A macro or path referenced in the spec no longer exists in the new sources

To recover:
1. Check the Koji task URL printed in the output to see the build log
2. Fix `$DISTGIT/ndctl.spec`
3. Commit and push the fix:
   ```bash
   git -C $DISTGIT add ndctl.spec
   git -C $DISTGIT commit -m "fix spec for vNN"
   (cd $DISTGIT && fedpkg push)
   ```
4. Re-run script 04 from the top — it will run a new scratch build first

---

### "Build already exists" error in Koji

Koji refuses to build an NVR that was already built.  The branch's spec is
still at the old version — usually meaning the release files never made it to
that branch.  Since script 04 copies the files from `main` itself, this
normally only happens if the push to `main` (script 03) landed somewhere else.
Check the branch and main:
```bash
git -C $DISTGIT log --oneline main | head -3     # should show "release vNN"
git -C $DISTGIT log --oneline <branch> | head -3
```
If `main` is missing the release commit, see "Script ran on the wrong branch"
below.  Once main is correct, retry the branch:
`br=<branch> ./contrib/fedora/04-do_build_branches vNN`

---

### A stable branch has diverged from `main`

This happens when commits land on a branch independently of main — e.g. a
Fedora mass rebuild, or a past manual fix-up.  It is expected and harmless:
script 04 copies the release files from main rather than merging, precisely so
that diverged branches work the same as clean ones.  No action needed.

---

### Bodhi update not submitted for a branch

Retry just that branch (script 05 verifies the build then submits):
```bash
br=<branch> ./contrib/fedora/05-do_submit_updates vNN
```
Or fully manually:
```bash
git -C $DISTGIT checkout <branch>
cd $DISTGIT && fedpkg update --request=testing --type=enhancement --notes=release-vNN
```

---

### An unexpected rawhide update appeared in bodhi

Not a problem.  Bodhi automatically creates an update for rawhide when the
main build completes — nobody submits it, and script 05 deliberately skips
rawhide for this reason.

---

### Script ran on the wrong branch

If scripts 01-03 ran while dist-git was on a stable branch (e.g. `f43`) instead
of `main`, the new version commit landed on that branch only.  `main` still has
the old version, and script 04 copies from `main`, so it would propagate the
old version.

To fix: copy the files from the branch where the commit landed onto `main`:
```bash
git -C $DISTGIT checkout main
git -C $DISTGIT checkout f43 -- ndctl.spec .gitignore sources
git -C $DISTGIT add ndctl.spec .gitignore sources
git -C $DISTGIT commit -m "release vNN"
git -C $DISTGIT push
```
Then proceed normally with script 04.
