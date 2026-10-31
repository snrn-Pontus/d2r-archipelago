"""The Archipelago Launcher's "Diablo II Resurrected Options" button: an editor for a player's options YAML, with a
checkbox for each toggle, a dropdown for each choice, a slider for each range and a number for each item of a count
list.

Archipelago's own Options Creator covers the same options but only writes new files, so this one opens an existing
YAML, applies the presets from options.py and saves back. Every control comes from the option classes (their type,
range, choices and description), so a new option shows up without touching this file.

Options the file sets to something the editor can't show (weights, "random", unknown values) are kept as they are
unless changed here. Saving only rewrites the lines of options that changed, so the rest of the file (comments,
other games, triggers) stays as it was.
"""
import re
import threading
from functools import partial
from pathlib import Path
from typing import Any

from Options import Choice, NamedRange, Option, OptionCounter, OptionSet, Range, Toggle, Visibility

from .ids import CUBE_ACCESS, MERCHANT_ACCESS, TOWN_PORTAL_ACCESS
from .options import (OPTION_GROUPS, OPTION_PRESETS, BossRewardType, CharacterClass, D2ROptions, DeathLinkMode, Goal,
                      MonsterShuffle, SkillFocus, SkillMode, SuperUniqueShuffle)

GAME = "Diablo II Resurrected"
TITLE = f"{GAME} Options"


def option_kind(option: type[Option]) -> str | None:
    """Which control edits this option, or None if the editor has none for it."""
    if issubclass(option, Toggle):
        return "toggle"
    if issubclass(option, Choice):
        return "choice"
    if issubclass(option, Range):
        return "range"
    if issubclass(option, OptionSet) and option.valid_keys:
        return "set"
    if issubclass(option, OptionCounter) and option.valid_keys:
        return "counter"
    return None


# The options with a control here; the rest, like item and location lists, plando and item links, stay as the file
# has them.
OPTIONS: dict[str, type[Option]] = {name: option for name, option in D2ROptions.type_hints.items()
                                    if option.visibility & Visibility.simple_ui and option_kind(option) is not None}


def read_value(option: type[Option], raw: Any) -> Any:
    """The editor's value for a YAML value (bool, choice number, range number or sorted list of keys), or None when
    it has no single value the editor can show: weights over several values, random, or something invalid."""
    kind = option_kind(option)
    if kind is None:
        return None
    if isinstance(raw, dict) and option.supports_weighting:
        picked = [value for value, weight in raw.items() if weight]
        if len(picked) != 1:
            return None
        raw = picked[0]
    if isinstance(raw, str) and raw.strip().lower().startswith("random"):
        return None
    try:
        value = option.from_any(raw).value
    except Exception:
        return None
    if kind == "toggle":
        return bool(value)
    if kind == "set":
        return sorted(value) if set(value) <= set(option.valid_keys) else None
    if kind == "counter":
        fits = set(value) <= set(option.valid_keys) and all(counter_allows(option, count) for count in value.values())
        return counts(value) if fits else None
    if kind == "range" and not range_allows(option, value):
        return None
    return value


def special_values(option: type[Option]) -> list[int]:
    return sorted(option.special_range_names.values()) if issubclass(option, NamedRange) else []


def range_allows(option: type[Option], value: int) -> bool:
    """A range takes its span and its named values, which may lie outside it (like 0 for "same")."""
    return option.range_start <= value <= option.range_end or value in special_values(option)


def counts(value: dict[str, int]) -> dict[str, int]:
    """A counter's editor value: its keys in order, without the zeros."""
    return {key: count for key, count in sorted(value.items()) if count}


def counter_allows(option: type[Option], count: Any) -> bool:
    """A whole number within the counter's limits (older Archipelago versions don't check the type)."""
    if not isinstance(count, int) or isinstance(count, bool):
        return False
    return (option.min is None or count >= option.min) and (option.max is None or count <= option.max)


def snap_range(option: type[Option], value: int) -> int:
    """The nearest value the range takes, for a slider that also spans named values outside it."""
    if range_allows(option, value):
        return value
    return min([option.range_start, option.range_end, *special_values(option)], key=lambda allowed: abs(allowed - value))


def write_value(option: type[Option], value: Any) -> Any:
    """The YAML value for an editor value: names for choices and named range values."""
    kind = option_kind(option)
    if kind == "choice":
        return option.name_lookup[value]
    if kind == "range" and issubclass(option, NamedRange):
        return next((name for name, number in option.special_range_names.items() if number == value), value)
    if kind == "set":
        return sorted(value)
    if kind == "counter":
        return counts(value)
    return value


def default_value(option: type[Option]) -> Any:
    value = read_value(option, option.default)
    if value is not None:
        return value
    return option.range_start if issubclass(option, Range) else None


def linked_changes(name: str, value: Any) -> dict[str, Any]:
    """Other options a change settles, so the editor shows what the seed will do: Skill Tree Shuffle needs (and turns
    on) Skill Mode tree."""
    if name == "skill_tree_shuffle" and value:
        return {"skill_mode": SkillMode.option_tree}
    if name == "skill_mode" and value != SkillMode.option_tree:
        return {"skill_tree_shuffle": False}
    return {}


LOCK_TOGGLES = {CUBE_ACCESS: "cube_lock", TOWN_PORTAL_ACCESS: "town_portal_lock", "Potion Access": "potion_lock",
                "Stash Access": "stash_lock", MERCHANT_ACCESS: "merchant_lock"}


def needed_toggle(name: str, key: str) -> str | None:
    """The toggle a set option's entry needs on, so the editor only lets you tick it then: an early lock needs its lock
    (Merchant Access in Early Locks needs Merchant Lock)."""
    if name not in ("early_locks", "local_early_locks"):
        return None
    return "waypoint_lock" if key.endswith(" Waypoints") else LOCK_TOGGLES.get(key)


def tree_mode(values: dict[str, Any]) -> bool:
    return values["skill_mode"] == SkillMode.option_tree or bool(values["skill_tree_shuffle"])


def death_link_traps(values: dict[str, Any]) -> bool:
    """DeathLink's random_trap mode sets off the seed's traps (by their weights, Junk Backpack aside)."""
    return bool(values["death_link"]) and values["death_link_mode"] == DeathLinkMode.option_random_trap


def traps_on(values: dict[str, Any]) -> bool:
    return values["trap_percentage"] > 0 or death_link_traps(values)


# The traps that go off where you are (and can wait for town); a Junk Backpack fills the inventory at once.
COMMAND_TRAPS = ("ambush_weight", "poison_cloud_weight", "firestorm_weight", "mana_drain_weight", "tax_collector_weight",
                 "mercenary_down_weight")
TRAP_PARENTS = ("trap_percentage", "death_link", "death_link_mode", *COMMAND_TRAPS)


def trap_goes_off(values: dict[str, Any], weight: str) -> bool:
    """Whether a command trap can go off: by its weight, from the pool or DeathLink random_trap, or as one of
    random_trap's fallbacks (Ambush, Poison Cloud, Firestorm, Mana Drain, Tax Collector) with every weight at 0."""
    if values[weight] > 0:
        return traps_on(values)
    return (death_link_traps(values) and weight != "mercenary_down_weight"
            and all(values[other] == 0 for other in COMMAND_TRAPS))


def boss_reward(values: dict[str, Any], *rewards: int) -> bool:
    return bool(values["boss_quests_give_uniques"]) and values["boss_reward_type"] in rewards


CODEX = "Needs Skill Mode codex"
# Option -> (the options it hangs on, what it needs, whether it applies), for options that do nothing unless another
# option is set. The editor greys them out and says what they need; their values stay, and still save.
APPLIES: dict[str, tuple[tuple[str, ...], str, Any]] = {
    **{name: (("act_keys",), "Needs Act Keys", lambda v: bool(v["act_keys"]))
       for name in ("enforce_act_keys", "act_key_window")},
    "enforce_act_keys_mode": (("act_keys", "enforce_act_keys"), "Needs Enforce Act Keys",
                              lambda v: bool(v["act_keys"] and v["enforce_act_keys"])),
    **{name: (("skill_mode", "skill_tree_shuffle"), CODEX, lambda v: not tree_mode(v))
       for name in ("skill_pool_size", "skill_focus", "skill_classes", "scale_with_length", "skill_level_scaling",
                    "codex_empowerments", "levels_per_copy", "codex_level_cap")},
    "skill_scaling_cap": (("skill_mode", "skill_tree_shuffle", "skill_level_scaling"),
                          "Needs Skill Mode codex and Skill Level Scaling above 0",
                          lambda v: not tree_mode(v) and v["skill_level_scaling"] > 0),
    "levels_per_empowerment": (("skill_mode", "skill_tree_shuffle", "codex_empowerments"),
                               "Needs Skill Mode codex and Codex Empowerments above 0",
                               lambda v: not tree_mode(v) and v["codex_empowerments"] > 0),
    "class_weight_percent": (("skill_mode", "skill_tree_shuffle", "skill_focus", "character_class"),
                             "Needs Skill Mode codex, Skill Focus class_weighted and a Character Class",
                             lambda v: not tree_mode(v) and v["skill_focus"] == SkillFocus.option_class_weighted
                             and v["character_class"] != CharacterClass.option_any),
    "include_warlock": (("character_class",), "Needs a Character Class other than warlock (which turns it on)",
                        lambda v: v["character_class"] != CharacterClass.option_warlock),
    "skill_points": (("skill_mode", "skill_tree_shuffle"), "Needs Skill Mode tree", tree_mode),
    "useful_starting_skills": (("starting_skills",), "Needs Starting Skills above 0", lambda v: v["starting_skills"] > 0),
    **{name: (("level_checks",), "Needs Level Checks", lambda v: bool(v["level_checks"]))
       for name in ("level_check_step", "grind_level")},
    **{name: (("goal", "goal_act"), "Needs Goal Difficulty hell and Goal Act 5",
              lambda v: v["goal"] == Goal.option_hell and v["goal_act"] == 5)
       for name in ("uber_checks", "special_item_checks")},
    "kill_count_multiplier": (("kill_count_checks",), "Needs Kill Count Checks", lambda v: bool(v["kill_count_checks"])),
    "find_count_multiplier": (("find_count_checks",), "Needs Find Count Checks", lambda v: bool(v["find_count_checks"])),
    "cow_level_checks": (("goal",), "Needs a goal past Normal (cows count on a difficulty finished before the goal)",
                         lambda v: v["goal"] > Goal.option_normal),
    "cow_kill_count": (("cow_level_checks", "goal"), "Needs Cow Level Checks and a goal past Normal",
                       lambda v: bool(v["cow_level_checks"]) and v["goal"] > Goal.option_normal),
    "tp_access_tome_charges": (("town_portal_lock",), "Needs Town Portal Lock", lambda v: bool(v["town_portal_lock"])),
    "potion_lock_mode": (("potion_lock",), "Needs Potion Lock", lambda v: bool(v["potion_lock"])),
    "hint_balance": (("cain_hints", "page_hint_chance", "prophecy_shop", "prophecy_weight"),
                     "Needs a way to find hints (Cain Hints, Page Hint Chance, Prophecy Shop or Prophecy Weight)",
                     lambda v: bool(v["cain_hints"] or v["prophecy_shop"]) or v["page_hint_chance"] > 0
                     or v["prophecy_weight"] > 0),
    "boss_reward_type": (("boss_quests_give_uniques",), "Needs Boss Quests Give Uniques",
                         lambda v: bool(v["boss_quests_give_uniques"])),
    "gold_reward_per_level": (("gold_weight",), "Needs Gold Weight above 0", lambda v: v["gold_weight"] > 0),
    "experience_reward_percent": (("experience_weight",), "Needs Experience Weight above 0",
                                  lambda v: v["experience_weight"] > 0),
    **{name: (("unique_weight", "set_item_weight", "boss_quests_give_uniques", "boss_reward_type"),
              "Needs Unique Weight or Set Item Weight above 0, or unique or set boss rewards",
              lambda v: v["unique_weight"] > 0 or v["set_item_weight"] > 0 or boss_reward(v, *range(5)))
       for name in ("reward_level_below", "reward_level_above")},
    "jewel_share": (("charm_weight",), "Needs Charm Weight above 0", lambda v: v["charm_weight"] > 0),
    "rare_jewel_chance": (("charm_weight", "jewel_share"), "Needs Charm Weight and Jewel Share above 0",
                          lambda v: v["charm_weight"] > 0 and v["jewel_share"] > 0),
    "high_rune_curve": (("high_rune_weight", "boss_quests_give_uniques", "boss_reward_type"),
                        "Needs High Rune Weight above 0, or High Rune boss rewards",
                        lambda v: v["high_rune_weight"] > 0 or boss_reward(v, BossRewardType.option_high_rune)),
    "shrine_pool": (("shrine_weight",), "Needs Shrine Weight above 0", lambda v: v["shrine_weight"] > 0),
    **{name: (("trap_percentage", "death_link", "death_link_mode"),
              "Needs Trap Percentage above 0, or DeathLink Mode random_trap", traps_on)
       for name in COMMAND_TRAPS},
    "traps_wait_for_town": (TRAP_PARENTS, "Needs a trap other than Junk Backpack that can go off",
                            lambda v: any(trap_goes_off(v, weight) for weight in COMMAND_TRAPS)),
    "junk_backpack_weight": (("trap_percentage",), "Needs Trap Percentage above 0", lambda v: v["trap_percentage"] > 0),
    **{name: (TRAP_PARENTS, "Needs Ambush traps: Ambush Weight above 0, with Trap Percentage above 0 or DeathLink "
                            "Mode random_trap", lambda v: trap_goes_off(v, "ambush_weight"))
       for name in ("ambush_size", "ambush_monsters")},
    "tax_collector_gold": (TRAP_PARENTS, "Needs Tax Collector traps (Tax Collector Weight above 0, with Trap Percentage "
                                         "above 0 or DeathLink Mode random_trap), or DeathLink Mode lose_gold",
                           lambda v: trap_goes_off(v, "tax_collector_weight")
                           or (bool(v["death_link"]) and v["death_link_mode"] == DeathLinkMode.option_lose_gold)),
    "junk_backpack_size": (("trap_percentage", "junk_backpack_weight"),
                           "Needs Trap Percentage and Junk Backpack Weight above 0",
                           lambda v: v["trap_percentage"] > 0 and v["junk_backpack_weight"] > 0),
    "monster_shuffle_tolerance": (("monster_shuffle",), "Needs Monster Shuffle anywhere",
                                  lambda v: v["monster_shuffle"] == MonsterShuffle.option_anywhere),
    "shuffle_cow_level": (("monster_shuffle",), "Needs Monster Shuffle",
                          lambda v: v["monster_shuffle"] != MonsterShuffle.option_off),
    **{name: (("superunique_shuffle",), "Needs Superunique Shuffle",
              lambda v: v["superunique_shuffle"] != SuperUniqueShuffle.option_off)
       for name in ("main_path_superunique_cap", "main_path_superunique_minions")},
    "superunique_shuffle_spread": (("superunique_shuffle",), "Needs Superunique Shuffle nearby",
                                   lambda v: v["superunique_shuffle"] == SuperUniqueShuffle.option_nearby),
    "runeword_rune_spread": (("runeword_shuffle",), "Needs Runeword Shuffle", lambda v: bool(v["runeword_shuffle"])),
    **{name: (("death_link",), "Needs DeathLink", lambda v: bool(v["death_link"]))
       for name in ("death_link_mode", "death_link_amnesty")},
}


def inactive(name: str, values: dict[str, Any], kept: dict[str, Any]) -> str | None:
    """What an option needs when it does nothing with these values, or None when it applies. An option hanging on one
    the file sets to weights or random may apply, so it stays active."""
    if name not in APPLIES:
        return None
    parents, needs, applies = APPLIES[name]
    if any(parent in kept for parent in parents) or applies(values):
        return None
    return needs


def game_section(data: dict) -> dict:
    section = data.get(GAME)
    return section if isinstance(section, dict) else {}


def load_options(data: dict) -> tuple[dict[str, Any], dict[str, Any]]:
    """Editor values for every option, and the raw YAML values of the options the editor can't show (kept)."""
    section = game_section(data)
    values: dict[str, Any] = {}
    kept: dict[str, Any] = {}
    for name, option in OPTIONS.items():
        value = read_value(option, section[name]) if name in section else None
        if name in section and value is None:
            kept[name] = section[name]
        values[name] = default_value(option) if value is None else value
    # A file from before the shuffle turned on tree mode shows (and saves) what the seed does.
    if values["skill_tree_shuffle"] and "skill_tree_shuffle" not in kept and "skill_mode" not in kept:
        values.update(linked_changes("skill_tree_shuffle", True))
    return values, kept


def preset_values(preset: str | None) -> dict[str, Any]:
    """Editor values for a preset from options.py; None or an unknown name gives the defaults."""
    values, _ = load_options({GAME: OPTION_PRESETS.get(preset or "", {})})
    return values


def top_changes(data: dict, name: str) -> dict[str, Any]:
    """The top-level keys to write: the player name, the game (unless the file weighs several), and a description
    for a new file."""
    changes: dict[str, Any] = {}
    if data.get("name") != name:
        changes["name"] = name
    if not isinstance(data.get("game"), dict) and data.get("game") != GAME:
        changes["game"] = GAME
    if not data:
        changes["description"] = f"{GAME} options, made with the {TITLE} editor."
    return changes


def option_changes(data: dict, values: dict[str, Any], kept: dict[str, Any]) -> dict[str, Any]:
    """The YAML values of the options the file doesn't already say. A new file gets all of them; an existing one
    only gets an option it leaves out when it isn't the default."""
    section = game_section(data)
    changes: dict[str, Any] = {}
    for key, option in OPTIONS.items():
        if key in kept:
            continue
        if key in section:
            if read_value(option, section[key]) == values[key]:
                continue
        elif section and values[key] == default_value(option):
            continue
        changes[key] = write_value(option, values[key])
    return changes


def save_options(data: dict, name: str, values: dict[str, Any], kept: dict[str, Any]) -> dict:
    """The file's data with the player name and this game's options set; everything else is left as it was."""
    saved = {**data, **top_changes(data, name)}
    saved[GAME] = {**game_section(data), **option_changes(data, values, kept)}
    return saved


def flow(value: Any) -> str:
    """A value as it goes after "key: " on one line."""
    import Utils
    text = Utils.dump(value, default_flow_style=True, allow_unicode=True, width=1 << 20).strip()
    return text.removesuffix("...").strip()


def set_key(lines: list[str], start: int, end: int, indent: str, key: str, value: Any) -> int:
    """Sets key to value among lines[start:end] (keys at this indent), keeping its comment; a key that isn't there is
    added after the last line. Returns the new end."""
    pattern = re.compile(rf"""{re.escape(indent)}(?P<q>["']?){re.escape(key)}(?P=q)[ \t]*:(?P<rest>(?:[ \t].*)?)$""")
    for index in range(start, end):
        match = pattern.match(lines[index])
        if match is None:
            continue
        parts = re.match(r"(?P<space>[ \t]*)(?P<value>.*?)(?P<comment>[ \t]+#.*)?$", match["rest"])
        quote = match["q"]
        line = f"{indent}{quote}{key}{quote}:{parts['space'] or ' '}{flow(value)}"
        if parts["comment"]:
            # The comment keeps its column when it can.
            comment = parts["comment"].lstrip()
            line += " " * max(1, len(lines[index]) - len(comment) - len(line)) + comment
        lines[index] = line
        if not parts["value"]:
            # A block value (weights or a list) on the lines below: drop it.
            last = index
            for below in range(index + 1, end):
                line = lines[below]
                content = line.lstrip()
                if not content or content.startswith("#"):
                    continue
                depth = len(line) - len(content)
                if depth > len(indent) or (depth == len(indent) and content.startswith("- ")):
                    last = below
                else:
                    break
            del lines[index + 1:last + 1]
            end -= last - index
        return end
    # After the last line that isn't blank or a column-0 comment (which may belong to whatever follows).
    last = next((index for index in range(end - 1, start - 1, -1)
                 if lines[index].strip() and not lines[index].startswith("#")), start - 1)
    lines.insert(last + 1, f"{indent}{key}: {flow(value)}")
    return end + 1


def patch_yaml(text: str, top: dict[str, Any], options: dict[str, Any]) -> str:
    """The file's text with these top-level keys and game options set, leaving every other line (and comment) alone."""
    newline = "\r\n" if "\r\n" in text else "\n"
    lines = text.splitlines()
    header = re.compile(rf"""(["']?){re.escape(GAME)}\1[ \t]*:[ \t]*(#.*)?$""")
    start = next((index + 1 for index, line in enumerate(lines) if header.match(line)), None)
    if start is None:
        lines += ["", f"{GAME}:"]
        start = len(lines)
    end = next((index for index in range(start, len(lines))
                if lines[index][:1] not in ("", " ", "\t", "#")), len(lines))
    first = next((line for line in lines[start:end] if line.strip() and not line.lstrip().startswith("#")), "  x")
    indent = first[:len(first) - len(first.lstrip())] or "  "
    for key, value in options.items():
        end = set_key(lines, start, end, indent, key, value)
    for key, value in top.items():
        pattern = re.compile(rf"""(["']?){re.escape(key)}\1[ \t]*:""")
        if any(pattern.match(line) for line in lines):
            set_key(lines, 0, len(lines), "", key, value)
        else:
            # New top-level keys go above the first one, after the file's opening comments.
            first_key = next((index for index, line in enumerate(lines) if line.strip() and not line.startswith("#")),
                             len(lines))
            lines.insert(first_key, f"{key}: {flow(value)}")
    return newline.join(lines) + newline


def render_yaml(text: str | None, data: dict, name: str, values: dict[str, Any],
                kept: dict[str, Any]) -> tuple[dict, str]:
    """The saved data and the text to write. An opened file is patched line by line so its comments and layout stay;
    if the patch doesn't read back as exactly the saved data, the whole file is written out instead."""
    import Utils
    saved = save_options(data, name, values, kept)
    if text is not None:
        try:
            patched = patch_yaml(text, top_changes(data, name), option_changes(data, values, kept))
            if Utils.parse_yaml(patched) == saved:
                return saved, patched
        except Exception:
            pass
    return saved, Utils.dump(saved, sort_keys=False, allow_unicode=True)


def name_problem(name: str) -> str | None:
    if not name.strip():
        return "Enter a player name first."
    if len(name) > 16:
        return "The player name can't be longer than 16 characters."
    return None


def grouped_options() -> list[tuple[str, list[str]]]:
    """(group name, option names) in the order of OPTION_GROUPS; options in no group come first."""
    names = {option: name for name, option in OPTIONS.items()}
    grouped = [(group.name, [names[option] for option in group.options if option in names]) for group in OPTION_GROUPS]
    listed = {name for _, group in grouped for name in group}
    others = [name for name in OPTIONS if name not in listed]
    return ([("Game Options", others)] if others else []) + grouped


def describe(option: type[Option]) -> str:
    return " ".join(line.strip() for line in (option.__doc__ or "").strip().splitlines())


def run_editor(path: str | None = None) -> None:
    """Opens the editor window, with a file if given."""
    import Utils
    Utils.init_logging("D2ROptionsEditor")
    import kvui  # noqa: F401  (sets Kivy up for Archipelago; must come before any other Kivy import)
    from kivy.app import App
    from kivy.clock import Clock
    from kivy.core.window import Window
    from kivy.metrics import dp, sp
    from kivy.uix.boxlayout import BoxLayout
    from kivy.uix.button import Button
    from kivy.uix.checkbox import CheckBox
    from kivy.uix.gridlayout import GridLayout
    from kivy.uix.label import Label
    from kivy.uix.scrollview import ScrollView
    from kivy.uix.slider import Slider
    from kivy.uix.spinner import Spinner
    from kivy.uix.textinput import TextInput

    def wrapped(text: str, **kwargs) -> Label:
        label = Label(text=text, size_hint_y=None, halign="left", valign="top", **kwargs)
        label.bind(width=lambda widget, width: setattr(widget, "text_size", (width, None)),
                   texture_size=lambda widget, size: setattr(widget, "height", size[1]))
        label.text_size = (label.width, None)
        return label

    class OptionsEditor(App):
        def __init__(self) -> None:
            super().__init__()
            self.title = TITLE
            self.path: Path | None = None
            self.data: dict = {}
            self.text: str | None = None
            self.values, self.kept = load_options({})

        def build(self):
            Window.size = (dp(900), dp(800))
            root = BoxLayout(orientation="vertical", padding=dp(8), spacing=dp(6))
            bar = BoxLayout(size_hint_y=None, height=dp(36), spacing=dp(6))
            bar.add_widget(Label(text="Player name", size_hint_x=None, width=dp(90)))
            self.name_input = TextInput(multiline=False, write_tab=False)
            bar.add_widget(self.name_input)
            self.preset = Spinner(text="Preset", values=["Defaults", *OPTION_PRESETS], size_hint_x=None, width=dp(150))
            self.preset.bind(text=lambda _, text: self.apply_preset(text))
            bar.add_widget(self.preset)
            for text, action in (("Open...", self.open_file), ("Save", self.save), ("Save as...", self.save_as)):
                bar.add_widget(Button(text=text, size_hint_x=None, width=dp(90), on_release=lambda _, run=action: run()))
            root.add_widget(bar)
            self.status = wrapped("Set your options, then Save. Open... edits an existing YAML.", color=(0.8, 0.8, 0.8, 1))
            root.add_widget(self.status)
            self.list = GridLayout(cols=1, size_hint_y=None, spacing=dp(10), padding=(0, 0, dp(12), 0))
            self.list.bind(minimum_height=self.list.setter("height"))
            scroll = ScrollView(do_scroll_x=False, bar_width=dp(8), scroll_type=["bars", "content"])
            scroll.add_widget(self.list)
            root.add_widget(scroll)
            self.fill()
            return root

        def on_start(self) -> None:
            if path:
                self.load(Path(path))

        def say(self, text: str) -> None:
            self.status.text = text

        def fill(self) -> None:
            self.list.clear_widgets()
            # Sets an option's control from code, for linked_changes.
            self.setters: dict[str, Any] = {}
            # Called with a toggle's new value, for set entries that need it (needed_toggle).
            self.watchers: dict[str, list[Any]] = {}
            # Option -> its row and the "Needs ..." line shown while it doesn't apply (APPLIES).
            self.dependents: dict[str, tuple[BoxLayout, Label]] = {}
            for group, names in grouped_options():
                self.list.add_widget(wrapped(f"[b]{group}[/b]", markup=True, font_size=sp(19), color=(1, 0.8, 0.4, 1)))
                for name in names:
                    self.list.add_widget(self.option_row(name))
            self.grey_out()

        def grey_out(self) -> None:
            """Greys out the options that do nothing with the current values, with what they need."""
            for name, (row, needs) in self.dependents.items():
                reason = inactive(name, self.values, self.kept)
                row.disabled = reason is not None
                if reason is None and needs.parent:
                    row.remove_widget(needs)
                elif reason is not None:
                    needs.text = reason
                    if not needs.parent:
                        # Right under the option's name.
                        row.add_widget(needs, index=len(row.children) - 1)

        def option_row(self, name: str) -> BoxLayout:
            option = OPTIONS[name]
            row = BoxLayout(orientation="vertical", size_hint_y=None, spacing=dp(2))
            row.bind(minimum_height=row.setter("height"))
            line = BoxLayout(size_hint_y=None, height=dp(32), spacing=dp(8))
            line.add_widget(wrapped(getattr(option, "display_name", name), size_hint_x=None, width=dp(220), bold=True))
            row.add_widget(line)
            notes: list[Label] = []

            def changed(value: Any) -> None:
                self.values[name] = value
                if name in self.kept:
                    del self.kept[name]
                    for note in notes:
                        row.remove_widget(note)
                for other, linked in linked_changes(name, value).items():
                    if other in self.setters:
                        self.setters[other](linked)
                for watch in self.watchers.get(name, []):
                    watch(value)
                self.grey_out()

            def set_control(widget, attribute: str, shown: Any, value: Any) -> None:
                setattr(widget, attribute, shown)
                changed(value)

            kind = option_kind(option)
            value = self.values[name]
            if kind == "toggle":
                box = CheckBox(active=value, size_hint_x=None, width=dp(32))
                box.bind(active=lambda _, active: changed(active))
                self.setters[name] = lambda new: set_control(box, "active", bool(new), bool(new))
                line.add_widget(box)
                line.add_widget(Label())
            elif kind == "choice":
                labels = {option.get_option_name(number): number for number in option.name_lookup}
                spinner = Spinner(text=option.get_option_name(value), values=list(labels), size_hint_x=None, width=dp(220))
                spinner.bind(text=lambda _, text: changed(labels[text]))
                self.setters[name] = lambda new: set_control(spinner, "text", option.get_option_name(new), new)
                line.add_widget(spinner)
                line.add_widget(Label())
            elif kind == "range":
                def shown(number: int) -> str:
                    special = write_value(option, number)
                    return f"{number} ({special})" if isinstance(special, str) else str(number)

                tag = Label(text=shown(value), size_hint_x=None, width=dp(110))
                ends = [option.range_start, option.range_end, *special_values(option)]
                slider = Slider(min=min(ends), max=max(ends), step=1, value=value)

                def slid(bar: Slider, number: float) -> None:
                    allowed = snap_range(option, int(number))
                    if allowed != int(number):
                        bar.value = allowed
                        return
                    tag.text = shown(allowed)
                    changed(allowed)

                slider.bind(value=slid)
                line.add_widget(slider)
                line.add_widget(tag)
            elif kind == "set":
                chosen = set(value)
                grid = GridLayout(cols=4, size_hint_y=None, height=dp(32) * -(-len(option.valid_keys) // 4))
                for key in sorted(option.valid_keys):
                    cell = BoxLayout()
                    box = CheckBox(active=key in chosen, size_hint_x=None, width=dp(32))

                    def ticked(_, active: bool, key: str = key) -> None:
                        chosen.add(key) if active else chosen.discard(key)
                        changed(sorted(chosen))

                    box.bind(active=ticked)
                    needs = needed_toggle(name, key)
                    if needs:
                        # Greyed out while its toggle is off; turning the toggle off unticks it.
                        def toggled(on: Any, box: CheckBox = box) -> None:
                            box.disabled = not on
                            if not on:
                                box.active = False

                        box.disabled = not self.values.get(needs)
                        self.watchers.setdefault(needs, []).append(toggled)
                    cell.add_widget(box)
                    name_label = Label(text=str(key), halign="left", valign="middle")
                    name_label.bind(size=name_label.setter("text_size"))
                    cell.add_widget(name_label)
                    grid.add_widget(cell)
                row.add_widget(grid)
            elif kind == "counter":
                chosen_counts = dict(value)
                grid = GridLayout(cols=3, size_hint_y=None, height=dp(32) * -(-len(option.valid_keys) // 3))
                for key in sorted(option.valid_keys):
                    cell = BoxLayout(spacing=dp(4))
                    number = TextInput(text=str(chosen_counts.get(key, 0)), multiline=False, write_tab=False, input_filter="int",
                                       size_hint_x=None, width=dp(48))

                    def typed(field: TextInput, text: str, key: str = key) -> None:
                        try:
                            count = int(text)
                        except ValueError:
                            count = 0
                        allowed = max(option.min or 0, min(count, option.max if option.max is not None else count))
                        if text and allowed != count:
                            field.text = str(allowed)
                            return
                        chosen_counts[key] = allowed
                        changed(counts(chosen_counts))

                    number.bind(text=typed)
                    cell.add_widget(number)
                    name_label = Label(text=str(key), halign="left", valign="middle")
                    name_label.bind(size=name_label.setter("text_size"))
                    cell.add_widget(name_label)
                    grid.add_widget(cell)
                row.add_widget(grid)
            row.add_widget(wrapped(describe(option), font_size=sp(13), color=(0.7, 0.7, 0.7, 1)))
            if name in self.kept:
                note = wrapped(f"The file has {self.kept[name]!r} here, which is kept unless you change it.",
                               font_size=sp(13), color=(1, 0.6, 0.3, 1))
                notes.append(note)
                row.add_widget(note)
            if name in APPLIES:
                # Stays bright while the rest of the row is greyed out.
                needs = wrapped("", font_size=sp(13), color=(1, 0.6, 0.3, 1), disabled_color=(1, 0.6, 0.3, 1))
                self.dependents[name] = (row, needs)
            return row

        def apply_preset(self, preset: str) -> None:
            if preset == "Preset":
                return
            self.values, self.kept = preset_values(None if preset == "Defaults" else preset), {}
            self.fill()
            self.say(f"Applied {preset}. Everything not in it is back to its default.")

        def load(self, file: Path) -> None:
            import Utils
            try:
                text = file.read_text(encoding="utf-8-sig")
                data = Utils.parse_yaml(text)
            except Exception as error:
                self.say(f"Couldn't read {file}: {error}")
                return
            if not isinstance(data, dict):
                self.say(f"{file} isn't a player options file.")
                return
            self.path, self.data, self.text = file, data, text
            self.values, self.kept = load_options(data)
            self.name_input.text = str(data.get("name", ""))
            self.preset.text = "Preset"
            self.fill()
            other = data.get("game") if isinstance(data.get("game"), str) and data.get("game") != GAME else None
            notes = [f"Opened {file}."]
            if other:
                notes.append(f"It was for {other}; saving makes it a {GAME} file.")
            if self.kept:
                notes.append(f"{len(self.kept)} option(s) use weights or random and are kept as they are.")
            self.say(" ".join(notes))

        def in_background(self, ask, then) -> None:
            def work() -> None:
                try:
                    chosen = ask()
                except Exception as error:
                    Clock.schedule_once(lambda _: self.say(f"Couldn't open the file dialog: {error}"))
                    return
                if chosen:
                    Clock.schedule_once(lambda _: then(Path(chosen)))
            threading.Thread(target=work, daemon=True).start()

        def open_file(self) -> None:
            import Utils
            self.in_background(lambda: Utils.open_filename("Open Options File", [("YAML", [".yaml", ".yml"])],
                                                           str(self.path or "")), self.load)

        def save(self) -> None:
            if self.path is None:
                self.save_as()
            else:
                self.write(self.path)

        def save_as(self) -> None:
            import Utils
            if problem := name_problem(self.name_input.text):
                self.say(problem)
                return
            suggest = self.path or Path(Utils.user_path("Players")) / f"{Utils.get_file_safe_name(self.name_input.text)}.yaml"
            self.in_background(lambda: Utils.save_filename("Save Options File As", [("YAML", [".yaml"])], str(suggest)),
                               self.write)

        def write(self, file: Path) -> None:
            if problem := name_problem(self.name_input.text):
                self.say(problem)
                return
            if not file.suffix:
                file = file.with_suffix(".yaml")
            data, text = render_yaml(self.text, self.data, self.name_input.text.strip(), self.values, self.kept)
            try:
                file.write_text(text, encoding="utf-8", newline="")
            except OSError as error:
                self.say(f"Couldn't save {file}: {error}")
                return
            self.path, self.data, self.text = file, data, text
            self.say(f"Saved {file}.")

    OptionsEditor().run()


def launch_editor(*args: str) -> None:
    from worlds.LauncherComponents import launch
    yaml = next((arg for arg in args if arg.lower().endswith((".yaml", ".yml"))), None)
    # A partial rather than launch's args, which older Archipelago versions don't have.
    launch(partial(run_editor, yaml), name=TITLE)
