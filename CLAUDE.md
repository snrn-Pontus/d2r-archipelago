# Working on issues

How issues on snrn-Pontus/d2r-archipelago are handled, especially when several are worked at once.

1. **React when starting.** Before touching code, put 👀 on the issue so it shows as taken:
   `gh api repos/snrn-Pontus/d2r-archipelago/issues/<N>/reactions -f content=eyes`
2. **One worktree, branch and PR per issue.** Never put two issues in one branch. Branch from a freshly fetched
   `origin/master`: `git worktree add ../d2r-issue-<N> -b issue-<N>-<slug> origin/master` (or a subagent with
   `isolation: "worktree"`). Remove the worktree once its PR is merged. Issues that touch the same files
   (e.g. `options.py`, `plugin.cpp`) conflict: do them one after another, or rebase the later one after the first merges.
3. **PR.** The body starts `Closes #<N>.`, then bullets of what changed, what was tested (`build.cmd`, `ap_tests`,
   the apworld tests) and what still needs an in-game check (also added to `PLAYTEST.md`), then `@codex review`.
4. **Fix until Codex is happy.** Fix each finding, push, comment `@codex review` again, and repeat until Codex finds
   no major issues. If Codex is out of credits, stop and ask. Squash-merge.
5. **Release with a changelog** once the PRs are merged (one release can cover several issues), following
   [RELEASING.md](RELEASING.md): on an up-to-date master, `python -I tools/release.py bump x.y.z`, rewrite the
   drafted `CHANGELOG.md` section for players (each line ending in `(#<N>)`, draft marker removed), commit
   "Release x.y.z", then `python -I tools/release.py publish <archipelago-checkout>`, which builds, tags `vX.Y.Z`,
   pushes and creates the GitHub release with the files. Publish from the checkout the Archipelago checkout's
   `worlds/d2r_archipelago` links to (the main repo), not a worktree.
