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
5. **Release with a changelog** once the PRs are merged (one release can cover several issues):
   - Bump the version in `apworld/d2r_archipelago/archipelago.json` (`world_version`) and `PluginVersion` in
     `plugin/src/plugin.cpp`. They must match, or `tools/package_release.py` refuses.
   - Add `## x.y.z - YYYY-MM-DD` to `CHANGELOG.md` (create it on the first release), with player-facing lines
     each ending in `(#<N>)`.
   - Commit "Release x.y.z", tag `vX.Y.Z`, push both.
   - `python -I tools/package_release.py <archipelago-checkout>`, then
     `gh release create vX.Y.Z dist/D2R-Archipelago-x.y.z.zip` with that version's changelog section as the notes.
