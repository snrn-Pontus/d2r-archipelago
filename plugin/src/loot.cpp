#include "loot.h"

#include "game_data.h"

#include <algorithm>
#include <cstdio>
#include <iterator>
#include <span>
#include <vector>

namespace ap::loot {

namespace {

using D2RL::Items::Quality;

// A unique or set item counts as level-appropriate when its requirement is at
// most this far below the character, and no more than LevelAbove above it.
constexpr int LevelBelow = 12;
constexpr int LevelAbove = 2;

constexpr const char* PerfectGems[] = { "gpv", "gpb", "gpg", "gpr", "gpw", "gpy", "skz" };
// Cheap white gear for the Junk Backpack trap.
constexpr const char* Junk[] = { "ssd", "clb", "hax", "cap", "buc", "lgl", "lbt", "sbw", "jav", "lbl" };
constexpr const char* RuneNames[] = { "El", "Eld", "Tir", "Nef", "Eth", "Ith", "Tal", "Ral", "Ort", "Thul", "Amn", "Sol", "Shael", "Dol", "Hel", "Io",
	"Lum", "Ko", "Fal", "Lem", "Pul", "Um", "Mal", "Ist", "Gul", "Vex", "Ohm", "Lo", "Sur", "Ber", "Jah", "Cham", "Zod" };

auto Rune(uint32_t number) -> Pick {
	char code[5] {};
	std::snprintf(code, sizeof(code), "r%02u", number);
	return { .code = CodeFromText(code), .name = std::string(RuneNames[number - 1]) + " Rune" };
}

// Picks a level-appropriate unique or set item, widening the search when the
// window is empty (a level 1 character still gets the lowest items).
auto PickQualityItem(std::span<const data::QualityItem> items, Quality quality, int level, bool anyKind, data::Kind kind, std::mt19937& random) -> Pick {
	std::vector<const data::QualityItem*> matching;
	for (const auto& item : items) {
		if (anyKind || item.kind == kind) {
			matching.push_back(&item);
		}
	}
	if (matching.empty()) {
		return {};
	}
	std::vector<const data::QualityItem*> window;
	for (const auto* item : matching) {
		if (item->reqLevel >= level - LevelBelow && item->reqLevel <= level + LevelAbove) {
			window.push_back(item);
		}
	}
	if (window.empty()) {
		// Nothing in range: take the closest requirements to the character.
		std::ranges::sort(matching, {}, [level](const data::QualityItem* item) { return std::abs(static_cast<int>(item->reqLevel) - level); });
		window.assign(matching.begin(), matching.begin() + std::min<size_t>(matching.size(), 8));
	}
	const auto* chosen = window[random() % window.size()];
	return {
		.code      = CodeFromText(chosen->code),
		.quality   = quality,
		.record    = chosen->record,
		.itemLevel = static_cast<uint32_t>(std::max<int>(chosen->itemLevel, level)),
		.name      = chosen->name,
	};
}

auto PickBase(int level, std::mt19937& random) -> Pick {
	std::vector<const data::Base*> window;
	for (const auto& base : data::SocketBases) {
		if (base.reqLevel <= level + LevelAbove && base.reqLevel + 20 >= level) {
			window.push_back(&base);
		}
	}
	if (window.empty()) {
		for (const auto& base : data::SocketBases) {
			if (base.reqLevel <= std::max(level, 1) + LevelAbove) {
				window.push_back(&base);
			}
		}
	}
	if (window.empty()) {
		window.push_back(&data::SocketBases[0]);
	}
	const auto* base = window[random() % window.size()];
	const uint32_t sockets = 3 + random() % (base->sockets - 2);
	// A high item level lets the base hold its full socket count.
	return { .code = CodeFromText(base->code), .itemLevel = 90, .sockets = sockets, .name = std::to_string(sockets) + "-socket " + base->name };
}

} // namespace

auto CodeFromText(const char* text) -> uint32_t {
	char padded[4] = { ' ', ' ', ' ', ' ' };
	for (size_t i = 0; i < 4 && text[i] != '\0'; ++i) {
		padded[i] = text[i];
	}
	return D2RL::Items::MakeItemCode(padded[0], padded[1], padded[2], padded[3]);
}

auto Roll(Loot loot, int level, std::mt19937& random) -> Pick {
	level = std::clamp(level, 1, 99);
	switch (loot) {
		case Loot::FullRejuvenation: return { .code = CodeFromText("rvl"), .name = "Full Rejuvenation Potion" };
		case Loot::PerfectGem:       return { .code = CodeFromText(PerfectGems[random() % std::size(PerfectGems)]), .name = "Perfect Gem" };
		case Loot::LowRune:          return Rune(1 + random() % 10);
		case Loot::MidRune:          return Rune(11 + random() % 10);
		case Loot::HighRune: {
			// Pul (21) through Zod (33), each rarer than the one before.
			std::vector<double> weights;
			for (int number = 21; number <= 33; ++number) {
				weights.push_back(static_cast<double>((34 - number) * (34 - number)));
			}
			std::discrete_distribution<int> pick(weights.begin(), weights.end());
			return Rune(21 + static_cast<uint32_t>(pick(random)));
		}
		case Loot::RandomUnique:  return PickQualityItem(data::Uniques, Quality::Unique, level, true, data::Weapon, random);
		case Loot::UniqueWeapon:  return PickQualityItem(data::Uniques, Quality::Unique, level, false, data::Weapon, random);
		case Loot::UniqueArmor:   return PickQualityItem(data::Uniques, Quality::Unique, level, false, data::Armor, random);
		case Loot::UniqueJewelry: return PickQualityItem(data::Uniques, Quality::Unique, level, false, data::Jewelry, random);
		case Loot::SetItem:       return PickQualityItem(data::SetItems, Quality::Set, level, true, data::Weapon, random);
		case Loot::SocketedBase:  return PickBase(level, random);
		case Loot::JunkBackpack:  return { .code = CodeFromText(Junk[random() % std::size(Junk)]), .name = "junk" };
		default:                  return {};
	}
}

auto EstimateLevel(int difficulty, int act) -> int {
	static constexpr int DifficultyStart[] = { 1, 38, 68 };
	return DifficultyStart[std::clamp(difficulty, 0, 2)] + std::clamp(act, 0, 4) * 6;
}

} // namespace ap::loot
