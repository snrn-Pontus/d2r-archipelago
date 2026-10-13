"""Cuts a release: version bump, changelog, tag and GitHub release. See RELEASING.md for the whole flow.

    python -I tools/release.py bump <x.y.z>                  sets both versions and drafts the changelog section
    python -I tools/release.py notes [<x.y.z>]               prints a version's changelog section
    python -I tools/release.py publish <archipelago-folder>  checks, packages, tags, pushes and makes the release

publish runs on master with the "Release x.y.z" commit on top and a clean tree. It builds the download with
tools/package_release.py before tagging, so a failed build leaves no tag behind, then pushes master and the tag
together and attaches the zip, the .apworld and the options template to the GitHub release.
"""
import datetime
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
WORLD_JSON = ROOT / "apworld" / "d2r_archipelago" / "archipelago.json"
PLUGIN_CPP = ROOT / "plugin" / "src" / "plugin.cpp"
CHANGELOG = ROOT / "CHANGELOG.md"
DIST = ROOT / "dist"
GAME = "Diablo II Resurrected"
VERSION = re.compile(r"^\d+\.\d+\.\d+$")
WORLD_VERSION = re.compile(r'("world_version":\s*")([^"]+)(")')
PLUGIN_VERSION = re.compile(r'(PluginVersion = ")([^"]+)(")')
DRAFT = "<!-- draft: "


def git(*args: str) -> str:
    return subprocess.run(["git", *args], cwd=ROOT, check=True, capture_output=True, text=True).stdout.strip()


def edit(path: Path, pattern: re.Pattern, version: str) -> None:
    """Swaps the version in place, keeping the file's line endings."""
    text = path.read_bytes().decode("utf-8")
    text, count = pattern.subn(lambda m: m.group(1) + version + m.group(3), text, count=1)
    if count != 1:
        sys.exit(f"No version found in {path}.")
    path.write_bytes(text.encode("utf-8"))


def current_version() -> str:
    world = WORLD_VERSION.search(WORLD_JSON.read_text(encoding="utf-8"))
    plugin = PLUGIN_VERSION.search(PLUGIN_CPP.read_text(encoding="utf-8"))
    if world is None or plugin is None or world.group(2) != plugin.group(2):
        sys.exit(f"The world's version ({world and world.group(2)}) and the plugin's ({plugin and plugin.group(2)}) don't match.")
    return world.group(2)


def key(version: str) -> tuple:
    return tuple(int(part) for part in version.split("."))


def last_tag() -> str | None:
    tags = [tag for tag in git("tag", "--list", "v*").splitlines() if VERSION.match(tag[1:])]
    return max(tags, key=lambda tag: key(tag[1:])) if tags else None


def section(version: str) -> str | None:
    """The changelog text under a version's heading, without the heading."""
    if not CHANGELOG.exists():
        return None
    match = re.search(rf"^## {re.escape(version)} - .*?$\n(.*?)(?=^## |\Z)", CHANGELOG.read_text(encoding="utf-8"), re.M | re.S)
    return match.group(1).strip() if match else None


def closed_issues(pr: str) -> list[str]:
    """The issues a PR closes, or the PR itself when it closes none (or gh can't say)."""
    result = subprocess.run(["gh", "pr", "view", pr, "--json", "closingIssuesReferences", "--jq", ".closingIssuesReferences[].number"],
                            cwd=ROOT, capture_output=True, text=True)
    return result.stdout.split() if result.returncode == 0 and result.stdout.split() else [pr]


def draft_lines(since: str | None) -> list[str]:
    """One line per commit since the last release. Squash merges end in "(#pr)", or "(#issue) (#pr)" when the
    title names its issue; the line keeps the issues the PR closes."""
    subjects = git("log", "--format=%s", f"{since}..HEAD" if since else "HEAD").splitlines()
    lines = []
    for subject in subjects:
        if subject.startswith("Release "):
            continue
        refs = re.findall(r"\(#(\d+)\)", subject)
        title = re.sub(r"\s*\(#\d+\)", "", subject).strip()
        issues = closed_issues(refs[-1]) if refs else []
        lines.append(f"- {title}" + "".join(f" (#{issue})" for issue in issues))
    return lines


def bump(version: str) -> None:
    if not VERSION.match(version):
        sys.exit(f"{version} isn't x.y.z.")
    if git("status", "--porcelain"):
        sys.exit("The tree has uncommitted changes; the release commit must hold only the bump and the changelog.")
    old = current_version()
    tag = last_tag()
    if key(version) <= key(tag[1:] if tag else "0.0.0") or key(version) < key(old):
        sys.exit(f"{version} isn't newer than {tag or old}.")
    if section(version) is not None:
        sys.exit(f"CHANGELOG.md already has a {version} section.")

    edit(WORLD_JSON, WORLD_VERSION, version)
    edit(PLUGIN_CPP, PLUGIN_VERSION, version)

    heading = f"## {version} - {datetime.date.today().isoformat()}"
    lines = draft_lines(tag)
    body = "\n".join([heading, "", f"{DRAFT}rewrite these for players, drop what they won't notice, then remove this line -->", *lines])
    if CHANGELOG.exists():
        text = CHANGELOG.read_text(encoding="utf-8")
        first = re.search(r"^## ", text, re.M)
        text = text[: first.start()] + body + "\n\n" + text[first.start():] if first else text.rstrip() + "\n\n" + body + "\n"
    else:
        text = "# Changelog\n\nWhat changed for players in each release. The numbers are GitHub issues.\n\n" + body + "\n"
    CHANGELOG.write_text(text, encoding="utf-8")
    print(f"{old} -> {version}; drafted {len(lines)} changelog lines in CHANGELOG.md since {tag or 'the first commit'}.")
    git("add", str(CHANGELOG))
    print(f'Edit them, then: git commit -am "Release {version}" and python -I tools/release.py publish <archipelago-folder>.')


def notes(version: str | None) -> None:
    version = version or current_version()
    text = section(version)
    if text is None:
        sys.exit(f"CHANGELOG.md has no {version} section.")
    print(text)


def check_archipelago(archipelago: Path) -> None:
    """build_apworld.py builds whatever worlds/d2r_archipelago links to, which must be this checkout's world."""
    world = archipelago / "worlds" / "d2r_archipelago"
    if not world.exists():
        sys.exit(f"{world} is missing; link it to {ROOT / 'apworld' / 'd2r_archipelago'}.")
    if world.resolve() != (ROOT / "apworld" / "d2r_archipelago").resolve():
        sys.exit(f"{world} links to {world.resolve()}, not this checkout. Publish from that checkout, or relink it.")


def publish(archipelago: Path) -> None:
    version = current_version()
    tag = f"v{version}"
    check_archipelago(archipelago)

    if git("rev-parse", "--abbrev-ref", "HEAD") != "master":
        sys.exit("Publish from master.")
    if git("status", "--porcelain"):
        sys.exit("The tree has uncommitted changes.")
    if git("log", "-1", "--format=%s") != f"Release {version}":
        sys.exit(f'The top commit must be "Release {version}" (bump, edit CHANGELOG.md, commit).')
    git("fetch", "--quiet", "--tags", "origin")
    if git("rev-parse", "HEAD~1") != git("rev-parse", "origin/master"):
        sys.exit("The release commit must sit right on top of origin/master; pull or rebase first.")
    if git("tag", "--list", tag):
        sys.exit(f"{tag} already exists.")
    text = section(version)
    if not text:
        sys.exit(f"CHANGELOG.md has no {version} section, or it's empty.")
    if DRAFT in text:
        sys.exit(f"CHANGELOG.md's {version} section is still a draft; rewrite it and remove the draft line.")

    subprocess.run([sys.executable, "-I", str(ROOT / "tools" / "package_release.py"), str(archipelago)], cwd=ROOT, check=True)
    files = [DIST / f"D2R-Archipelago-{version}.zip", DIST / "d2r_archipelago.apworld", DIST / f"{GAME}.yaml"]
    for file in files:
        if not file.exists():
            sys.exit(f"{file} wasn't built.")

    git("tag", "-a", tag, "-m", f"Release {version}")
    if subprocess.run(["git", "push", "--atomic", "origin", "master", tag], cwd=ROOT).returncode != 0:
        git("tag", "-d", tag)
        sys.exit(f"The push failed, so {tag} was removed again. Rebase the release commit onto origin/master and rerun publish.")
    with tempfile.NamedTemporaryFile("w", suffix=".md", delete=False, encoding="utf-8") as notes_file:
        notes_file.write(text + "\n")
    try:
        subprocess.run(["gh", "release", "create", tag, *map(str, files), "--title", version, "--notes-file", notes_file.name],
                       cwd=ROOT, check=True)
    finally:
        Path(notes_file.name).unlink()


def main() -> None:
    args = sys.argv[1:]
    if args[:1] == ["bump"] and len(args) == 2:
        bump(args[1])
    elif args[:1] == ["notes"] and len(args) <= 2:
        notes(args[1] if len(args) == 2 else None)
    elif args[:1] == ["publish"] and len(args) == 2:
        publish(Path(args[1]).resolve())
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
