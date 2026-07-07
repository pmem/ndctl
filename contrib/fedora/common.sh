# contrib/fedora/common.sh
#
# Shared functions and setup sourced by all ndctl Fedora packaging scripts.
# Not meant to be run directly.
#

# Locate the directory containing these scripts (contrib/fedora/ in the
# upstream clone), regardless of where the caller's working directory is.
SCRIPTS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Load local config (DISTGIT path etc.)
# If config.local is missing, print a clear setup message and exit.
load_config()
{
	if [[ ! -f "$SCRIPTS_DIR/config.local" ]]; then
		printf "ERROR: missing %s/config.local\n" "$SCRIPTS_DIR"
		printf "One-time setup: copy the example and fill in your paths:\n"
		printf "  cp %s/config.local.example %s/config.local\n" "$SCRIPTS_DIR" "$SCRIPTS_DIR"
		exit 1
	fi
	# shellcheck source=config.local.example
	source "$SCRIPTS_DIR/config.local"

	if [[ ! -d "$DISTGIT" ]]; then
		printf "ERROR: DISTGIT directory not found: %s\n" "$DISTGIT"
		printf "Check the DISTGIT path in %s/config.local\n" "$SCRIPTS_DIR"
		exit 1
	fi
}

# verify_upstream_repo: confirm we are running from inside the upstream repo.
# All scripts run from the upstream clone (not the dist-git clone).
#
# NOTE: The upstream repo's *branch* does not matter -- script 01 downloads
# the release tarball by tag from GitHub and never reads the local upstream
# tree.  The branch that matters is dist-git being on 'main', which is
# checked separately by verify_distgit_branch.
verify_upstream_repo()
{
	local upstream_dir
	upstream_dir="$(git -C "$SCRIPTS_DIR" rev-parse --show-toplevel 2>/dev/null)"

	# Make sure the caller's cwd is inside the upstream repo
	local cwd_toplevel
	cwd_toplevel="$(git rev-parse --show-toplevel 2>/dev/null)"

	if [[ "$cwd_toplevel" != "$upstream_dir" ]]; then
		printf "ERROR: must run from inside the upstream ndctl repo\n"
		printf "  Expected: %s\n" "$upstream_dir"
		printf "  Got:      %s\n" "$cwd_toplevel"
		exit 1
	fi
}

# verify_distgit_branch: confirm the dist-git clone is on the expected branch.
verify_distgit_branch()
{
	local expected_branch="$1"
	local current_branch
	current_branch="$(git -C "$DISTGIT" rev-parse --abbrev-ref HEAD)"
	if [[ "$current_branch" != "$expected_branch" ]]; then
		printf "ERROR: dist-git repo must be on '%s' branch, currently on '%s'\n" \
			"$expected_branch" "$current_branch"
		printf "Run: git -C %s checkout %s\n" "$DISTGIT" "$expected_branch"
		exit 1
	fi
}

# preflight_check: run all pre-flight checks and print a summary.
# Call this with --check to test the setup without doing any real work.
# Each script calls this at the start.
preflight_check()
{
	local ver="$1"
	printf "=== Pre-flight checks ===\n"
	printf "  Scripts dir:  %s\n" "$SCRIPTS_DIR"
	printf "  Dist-git dir: %s\n" "$DISTGIT"
	printf "  Version:      %s\n" "$ver"
	printf "  Upstream branch: %s\n" "$(git rev-parse --abbrev-ref HEAD)"
	printf "  Dist-git branch: %s\n" "$(git -C "$DISTGIT" rev-parse --abbrev-ref HEAD)"
	printf "=========================\n"
}
