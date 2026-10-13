#pragma once

#include <span>

// The debug starter kit (ap-debug kit, or Starter Kit on the debug panel): the
// items the playtest checklist keeps needing, for a fresh character. They're
// made as rewards, so none of them sends a check.

namespace ap::starter_kit {

struct Entry {
	const char* code;
	int         count;
	const char* name;
};

inline constexpr Entry Items[] = {
	// Cube recipes (the essences into a Token) and the Cube lock; extra room for the kit.
	{ "box", 1, "Horadric Cube" },
	// The tree-mode respec and its "nothing happens in the stash or Cube" checks.
	{ "toa", 1, "Token of Absolution" },
	// Cube these four into a second Token.
	{ "tes", 1, "Twisted Essence of Suffering" },
	{ "ceh", 1, "Charged Essence of Hatred" },
	{ "bet", 1, "Burning Essence of Terror" },
	{ "fed", 1, "Festering Essence of Destruction" },
	// Identifies the reward and found uniques without a trip to Cain.
	{ "ibk", 1, "Tome of Identify" },
	// Survives the tests without god mode; the Potion lock sets them aside until Potion Access.
	{ "rvl", 3, "Full Rejuvenation Potion" },
};

inline auto All() -> std::span<const Entry> {
	return Items;
}

} // namespace ap::starter_kit
