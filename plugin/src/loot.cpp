#include "loot.h"

#include "game_data.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <span>
#include <vector>

namespace ap::loot {

namespace {

using D2RL::Items::Quality;

// A socketed base can require at most this many levels over the character's.
constexpr int BaseLevelAbove = 2;

// Whether a character of this class can use the item (-1: class unknown).
bool Usable(int8_t itemClass, int playerClass) {
	return itemClass < 0 || playerClass < 0 || itemClass == playerClass;
}

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
auto PickQualityItem(std::span<const data::QualityItem> items, Quality quality, int level, bool anyKind, data::Kind kind, int playerClass,
                     const RewardSettings& rewards, std::mt19937& random) -> Pick {
	const int below = std::max(rewards.levelBelow, 0);
	const int above = std::max(rewards.levelAbove, 0);
	std::vector<const data::QualityItem*> matching;
	for (const auto& item : items) {
		if ((anyKind || item.kind == kind) && Usable(item.cls, playerClass)) {
			matching.push_back(&item);
		}
	}
	if (matching.empty()) {
		return {};
	}
	std::vector<const data::QualityItem*> window;
	for (const auto* item : matching) {
		if (item->reqLevel >= level - below && item->reqLevel <= level + above) {
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

auto PickBase(int level, int playerClass, std::mt19937& random) -> Pick {
	std::vector<const data::Base*> window;
	for (const auto& base : data::SocketBases) {
		if (base.sockets >= 2 && Usable(base.cls, playerClass) && base.reqLevel <= level + BaseLevelAbove && base.reqLevel + 20 >= level) {
			window.push_back(&base);
		}
	}
	if (window.empty()) {
		for (const auto& base : data::SocketBases) {
			if (base.sockets >= 2 && Usable(base.cls, playerClass) && base.reqLevel <= std::max(level, 1) + BaseLevelAbove) {
				window.push_back(&base);
			}
		}
	}
	if (window.empty()) {
		return {};
	}
	const auto* base = window[random() % window.size()];
	// Each count from two through this base's legal maximum is equally likely.
	const uint32_t sockets = std::uniform_int_distribution<uint32_t>(2, base->sockets)(random);
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

auto Roll(Loot loot, int level, std::mt19937& random, int playerClass, const RewardSettings& rewards) -> Pick {
	level = std::clamp(level, 1, 99);
	switch (loot) {
		case Loot::FullRejuvenation: return { .code = CodeFromText("rvl"), .name = "Full Rejuvenation Potion" };
		case Loot::PerfectGem:       return { .code = CodeFromText(PerfectGems[random() % std::size(PerfectGems)]), .name = "Perfect Gem" };
		case Loot::LowRune:          return Rune(1 + random() % 10);
		case Loot::MidRune:          return Rune(11 + random() % 10);
		case Loot::HighRune: {
			// Pul (21) through Zod (33), each (with a curve above 0) rarer than the one before.
			const int curve = std::clamp(rewards.highRuneCurve, 0, 4);
			std::vector<double> weights;
			for (int number = 21; number <= 33; ++number) {
				weights.push_back(std::pow(34.0 - number, curve));
			}
			std::discrete_distribution<int> pick(weights.begin(), weights.end());
			return Rune(21 + static_cast<uint32_t>(pick(random)));
		}
		case Loot::RandomUnique:  return PickQualityItem(data::Uniques, Quality::Unique, level, true, data::Weapon, playerClass, rewards, random);
		case Loot::UniqueWeapon:  return PickQualityItem(data::Uniques, Quality::Unique, level, false, data::Weapon, playerClass, rewards, random);
		case Loot::UniqueArmor:   return PickQualityItem(data::Uniques, Quality::Unique, level, false, data::Armor, playerClass, rewards, random);
		case Loot::UniqueJewelry: return PickQualityItem(data::Uniques, Quality::Unique, level, false, data::Jewelry, playerClass, rewards, random);
		case Loot::SetItem:       return PickQualityItem(data::SetItems, Quality::Set, level, true, data::Weapon, playerClass, rewards, random);
		case Loot::SocketedBase:  return PickBase(level, playerClass, random);
		case Loot::JunkBackpack:  return { .code = CodeFromText(Junk[random() % std::size(Junk)]), .name = "junk" };
		case Loot::RandomCharm: {
			constexpr const char* Charms[][2] = { { "cm1", "Small Charm" }, { "cm2", "Large Charm" }, { "cm3", "Grand Charm" } };
			const auto& charm = Charms[random() % std::size(Charms)];
			return { .code = CodeFromText(charm[0]), .quality = Quality::Magic, .itemLevel = static_cast<uint32_t>(level), .name = charm[1] };
		}
		case Loot::RandomJewel: {
			const bool rare = static_cast<int>(random() % 100) < rewards.rareJewelPercent;
			return { .code = CodeFromText("jew"), .quality = rare ? Quality::Rare : Quality::Magic, .itemLevel = static_cast<uint32_t>(level),
				     .name = rare ? "Rare Jewel" : "Magic Jewel" };
		}
		default:                  return {};
	}
}

auto GoldAmount(int level, const RewardSettings& rewards) -> int {
	return std::clamp(level, 1, 99) * std::clamp(rewards.goldPerLevel, 0, 10000);
}

auto ExperienceAmount(int level, const RewardSettings& rewards) -> int64_t {
	level = std::clamp(level, 1, 99);
	if (level >= 99) {
		return 0;
	}
	return (static_cast<int64_t>(data::Experience[level]) - data::Experience[level - 1]) * std::clamp(rewards.experiencePercent, 0, 100) / 100;
}

auto TaxAmount(int level, int goldPerLevel) -> int {
	return std::clamp(level, 1, 99) * std::max(goldPerLevel, 0);
}

auto ShrineId(std::mt19937& random, std::span<const int> pool) -> int {
	std::vector<int> shrines;
	for (const int id : pool) {
		if (std::ranges::find(HelpfulShrines, id) != std::end(HelpfulShrines) && std::ranges::find(shrines, id) == shrines.end()) {
			shrines.push_back(id);
		}
	}
	if (shrines.empty()) {
		shrines.assign(std::begin(HelpfulShrines), std::end(HelpfulShrines));
	}
	return shrines[random() % shrines.size()];
}

auto AmbushMonster(int act, std::mt19937& random) -> const char* {
	static constexpr const char* Monsters[5][4] = {
		{ "fallen1", "skeleton1", "zombie1", "brute1" },
		{ "sandraider1", "mummy1", "clawviper1", "scarab1" },
		{ "fetish1", "thornhulk1", "batdemon1", "fetish1" },
		{ "doomknight1", "megademon1", "regurgitator1", "doomknight1" },
		{ "reanimatedhorde1", "deathmauler1", "snowyeti1", "minion1" },
	};
	return Monsters[std::clamp(act, 0, 4)][random() % 4];
}

auto CommandsFor(Loot loot, int level, int act, std::mt19937& random, const TrapSettings& traps, std::span<const std::string> areaMonsters,
                 const RewardSettings& rewards) -> Commands {
	switch (loot) {
		// Spawns go to D2RCore's spawn location; "loc player" puts it on the player.
		case Loot::RandomShrine:
			return { { "loc player", "shrine " + std::to_string(ShrineId(random, rewards.shrines)) }, "a shrine appears at your feet." };
		case Loot::AmbushTrap: {
			const std::string monster = areaMonsters.empty() ? AmbushMonster(act, random) : areaMonsters[random() % areaMonsters.size()];
			return { { "loc player", "mspawn " + monster + "," + std::to_string(std::max(traps.ambushSize, 1)) }, "it's a trap! An ambush!", true };
		}
		case Loot::PoisonCloudTrap:  return { { "pcloud" }, "it's a trap! Poison gas!", true };
		case Loot::FirestormTrap:    return { { "fstorm" }, "it's a trap! Firestorm!", true };
		case Loot::ManaDrainTrap:    return { { "depletemana 0" }, "it's a trap! Your mana drains away.", true };
		case Loot::TaxCollectorTrap: {
			const int amount = TaxAmount(level, traps.taxPerLevel);
			return { { "gold -" + std::to_string(amount) }, "it's a trap! The tax collector takes up to " + std::to_string(amount) + " gold.", true };
		}
		case Loot::MercenaryDownTrap: return { { "killhirable" }, "it's a trap! Your mercenary falls.", true };
		default:                      return {};
	}
}

auto EstimateLevel(int difficulty, int act) -> int {
	static constexpr int DifficultyStart[] = { 1, 38, 68 };
	return DifficultyStart[std::clamp(difficulty, 0, 2)] + std::clamp(act, 0, 4) * 6;
}

} // namespace ap::loot
