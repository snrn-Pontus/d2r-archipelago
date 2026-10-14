#pragma once

#include <algorithm>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// The seed's starting gold and items (starting_gold, starting_items): handed out
// once per seed and slot, to the seed's character, as rewards (they send no
// checks). Gold goes through D2RCore like a Gold reward, items like loot.

namespace ap::starting {

struct Item {
	const char* code;
	const char* name;
};

// The items starting_items can ask for: small stackless things D2R makes
// whole with its default values. Must match STARTING_ITEMS in
// apworld/d2r_archipelago/options.py.
inline constexpr Item Items[] = {
	{ "hp1", "Minor Healing Potion" }, { "hp2", "Light Healing Potion" }, { "hp3", "Healing Potion" },
	{ "hp4", "Greater Healing Potion" }, { "hp5", "Super Healing Potion" },
	{ "mp1", "Minor Mana Potion" }, { "mp2", "Light Mana Potion" }, { "mp3", "Mana Potion" },
	{ "mp4", "Greater Mana Potion" }, { "mp5", "Super Mana Potion" },
	{ "rvs", "Rejuvenation Potion" }, { "rvl", "Full Rejuvenation Potion" },
	{ "vps", "Stamina Potion" }, { "yps", "Antidote Potion" }, { "wms", "Thawing Potion" },
	{ "tsc", "Scroll of Town Portal" }, { "isc", "Scroll of Identify" },
	{ "gpv", "Perfect Amethyst" }, { "gpb", "Perfect Sapphire" }, { "gpg", "Perfect Emerald" }, { "gpr", "Perfect Ruby" },
	{ "gpw", "Perfect Diamond" }, { "gpy", "Perfect Topaz" }, { "skz", "Perfect Skull" },
	{ "r01", "El Rune" }, { "r02", "Eld Rune" }, { "r03", "Tir Rune" }, { "r04", "Nef Rune" }, { "r05", "Eth Rune" },
	{ "r06", "Ith Rune" }, { "r07", "Tal Rune" }, { "r08", "Ral Rune" }, { "r09", "Ort Rune" }, { "r10", "Thul Rune" },
	{ "r11", "Amn Rune" }, { "r12", "Sol Rune" }, { "r13", "Shael Rune" }, { "r14", "Dol Rune" }, { "r15", "Hel Rune" },
	{ "r16", "Io Rune" }, { "r17", "Lum Rune" }, { "r18", "Ko Rune" }, { "r19", "Fal Rune" }, { "r20", "Lem Rune" },
	{ "r21", "Pul Rune" }, { "r22", "Um Rune" }, { "r23", "Mal Rune" }, { "r24", "Ist Rune" }, { "r25", "Gul Rune" },
	{ "r26", "Vex Rune" }, { "r27", "Ohm Rune" }, { "r28", "Lo Rune" }, { "r29", "Sur Rune" }, { "r30", "Ber Rune" },
	{ "r31", "Jah Rune" }, { "r32", "Cham Rune" }, { "r33", "Zod Rune" },
};

// The most copies of one item, and the most gold: what a level 1 character can
// carry, so none of it is lost. Must match the apworld's StartingItems and StartingGold.
inline constexpr int MaxCopies = 20;
inline constexpr int MaxGold = 10000;

inline auto Find(std::string_view code) -> const Item* {
	const auto found = std::ranges::find_if(Items, [&](const Item& item) { return code == item.code; });
	return found != std::end(Items) ? &*found : nullptr;
}

// The slot data's starting_items, (code, count), as one entry per copy in the
// slot data's order, so a save's count of copies handed out always means the
// same ones. Unknown codes are left out and counts held to 0..MaxCopies.
inline auto Expand(std::span<const std::pair<std::string, int>> entries) -> std::vector<const Item*> {
	std::vector<const Item*> copies;
	for (const auto& [code, count] : entries) {
		if (const Item* item = Find(code)) {
			copies.insert(copies.end(), static_cast<size_t>(std::clamp(count, 0, MaxCopies)), item);
		}
	}
	return copies;
}

// The starting gold still to give: none once given, and never more than MaxGold.
inline auto GoldToGive(int gold, bool given) -> int {
	return given ? 0 : std::clamp(gold, 0, MaxGold);
}

// Whether anything is still to hand out, given the gold flag and the copies handed out.
inline auto Pending(int gold, size_t copies, bool goldGiven, size_t copiesGiven) -> bool {
	return GoldToGive(gold, goldGiven) > 0 || copiesGiven < copies;
}

// One line naming what a pass handed out: "3 x Full Rejuvenation Potion, Ber Rune",
// in the order given, each item once.
inline auto Describe(std::span<const Item* const> given) -> std::string {
	std::vector<std::pair<const Item*, int>> counts;
	for (const Item* item : given) {
		const auto found = std::ranges::find_if(counts, [&](const auto& entry) { return entry.first == item; });
		if (found != counts.end()) {
			++found->second;
		} else {
			counts.emplace_back(item, 1);
		}
	}
	std::string text;
	for (const auto& [item, count] : counts) {
		text += (text.empty() ? "" : ", ") + (count > 1 ? std::to_string(count) + " x " : std::string {}) + item->name;
	}
	return text;
}

} // namespace ap::starting
