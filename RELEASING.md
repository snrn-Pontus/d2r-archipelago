# Releasing

A release is a `Release x.y.z` commit on master, a `vX.Y.Z` tag on it, and a GitHub release with the
player download and that version's `CHANGELOG.md` section as its notes. `tools/release.py` does the
mechanical parts and refuses to go on when something is off. One release can cover several merged PRs.

## Versions

The world (`world_version` in `apworld/d2r_archipelago/archipelago.json`) and the plugin (`PluginVersion` in
`plugin/src/plugin.cpp`) always carry the same version. The plugin warns when a seed's world has a different
major.minor, so:

- **minor** (0.6.0 -> 0.7.0) when seeds or the mod change: new options, items, locations or anything that
  needs a new seed or a rerun of `d2r-ap-setup`.
- **patch** (0.6.0 -> 0.6.1) for fixes that work with seeds made by the previous release.

## Steps

You need the [Archipelago source checkout](README.md#building) with `worlds/d2r_archipelago` linked to the
checkout you release from (publish checks this), Visual Studio Build Tools for the plugin, and `gh` signed in.

1. **Start from master.** All the release's PRs are merged; `git switch master` and `git pull`, tree clean.
2. **Bump.** `python -I tools/release.py bump x.y.z` sets both versions and adds a `## x.y.z - YYYY-MM-DD`
   section to `CHANGELOG.md`, drafted from the commits since the last tag, each ending in the issues its PR
   closed. It stages `CHANGELOG.md`, so a new one gets committed too.
3. **Write the changelog.** Rewrite the draft for players: what they'll notice, in plain words, one line each,
   ending in `(#<issue>)`. Drop internal work (docs, tests, refactors). Delete the `<!-- draft ... -->` line;
   publish won't go on while it's there. `python -I tools/release.py notes` shows the section as it will
   appear on GitHub.
4. **Commit.** `git commit -am "Release x.y.z"`. Don't push it yet.
5. **Publish.** `python -I tools/release.py publish <archipelago-checkout>`. It checks that it's on master
   with the release commit right on top of `origin/master`, a clean tree, matching versions, a finished
   changelog section and an unused tag; then builds and tests everything with `tools/package_release.py`,
   tags `vX.Y.Z`, pushes master and the tag together, and creates the GitHub release with these files:

   | File | For |
   |---|---|
   | `D2R-Archipelago-x.y.z.zip` | Players: the setup tool, the plugin, the apworld, the options template and the setup guide in one folder. |
   | `d2r_archipelago.apworld` | Hosts and players who only need the world (it carries the setup tool and plugin for its Launcher button). |
   | `Diablo II Resurrected.yaml` | The options template for this version. |

If the build fails, nothing is tagged or pushed: fix it on a branch, merge, and start again from step 1
(`git reset --hard origin/master` drops the unpushed release commit). If the push went through but
`gh release create` failed, rerun just that step:
`gh release create vX.Y.Z dist/D2R-Archipelago-x.y.z.zip dist/d2r_archipelago.apworld "dist/Diablo II Resurrected.yaml" --title x.y.z --notes "$(python -I tools/release.py notes)"`.
