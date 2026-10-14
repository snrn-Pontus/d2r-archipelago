#pragma once

// Fallback quest detection. D2RLoader's QuestCompleted event didn't fire in
// testing (Den of Evil on 1.3.1-beta), so quests also complete from signals
// the plugin can see: the quest's boss dying, its quest item turning up in
// your inventory, or reaching the area the quest leads to. The plugin logs
// which source completed each quest, to see whether the event works now
// (PLAYTEST.md section 2c). Ids are from D2R 3.3 (monstats *hcIdx, item
// codes, level ids).

#include "game_data.h"

#include <D2RLPlugin/item.h>

#include <cstdint>
#include <iterator>
#include <optional>

namespace ap::quests {

enum class Trigger : uint8_t { Kill, Item, Area };

struct QuestTrigger {
	Trigger  trigger;
	uint32_t id;   // monster class, item code or level id
	uint8_t  flag; // the quest's flag in data::Quests
};

inline constexpr uint32_t Code(const char (&text)[4]) {
	return D2RL::Items::MakeItemCode(text[0], text[1], text[2]);
}

inline constexpr QuestTrigger Triggers[] = {
	{ Trigger::Kill, 267, 2 },               // Blood Raven: Sisters' Burial Grounds
	{ Trigger::Item, Code("hdm"), 3 },       // Horadric Malus: Tools of the Trade
	{ Trigger::Area, 38, 4 },                // Tristram: The Search for Cain
	{ Trigger::Area, 25, 5 },                // Tower Cellar Level 5: The Forgotten Tower
	{ Trigger::Kill, 156, 6 },               // Andariel: Sisters to the Slaughter
	{ Trigger::Kill, 229, 9 },               // Radament: Radament's Lair
	{ Trigger::Item, Code("hst"), 10 },      // Horadric Staff: The Horadric Staff
	{ Trigger::Item, Code("vip"), 11 },      // Amulet of the Viper: Tainted Sun
	{ Trigger::Area, 74, 12 },               // Arcane Sanctuary: Arcane Sanctuary
	{ Trigger::Kill, 250, 13 },              // The Summoner: The Summoner
	{ Trigger::Kill, 211, 14 },              // Duriel: The Seven Tombs
	{ Trigger::Item, Code("bbb"), 17 },      // Lam Esen's Tome: Lam Esen's Tome
	{ Trigger::Item, Code("qf2"), 18 },      // Khalim's Will: Khalim's Will
	{ Trigger::Item, Code("g33"), 19 },      // The Gidbinn: Blade of the Old Religion
	{ Trigger::Item, Code("j34"), 20 },      // A Jade Figurine: The Golden Bird
	{ Trigger::Area, 100, 21 },              // Durance of Hate Level 1: The Blackened Temple
	{ Trigger::Kill, 242, 22 },              // Mephisto: The Guardian
	{ Trigger::Kill, 256, 25 },              // Izual: The Fallen Angel
	{ Trigger::Kill, 243, 26 },              // Diablo: Terror's End
	{ Trigger::Kill, 409, 27 },              // Hephasto: Hell's Forge
	{ Trigger::Area, 111, 35 },              // Frigid Highlands (past Shenk): Siege on Harrogath
	{ Trigger::Area, 112, 36 },              // Arreat Plateau (past the prisoners): Rescue on Mount Arreat
	{ Trigger::Item, Code("ice"), 37 },      // Malah's Potion: Prison of Ice
	{ Trigger::Kill, 526, 38 },              // Nihlathak: Betrayal of Harrogath
	{ Trigger::Area, 128, 39 },              // Worldstone Keep Level 1 (past the Ancients): Rite of Passage
	{ Trigger::Kill, 544, 40 },              // Baal: Eve of Destruction
};

// Den of Evil (flag 1) has no boss or item: it completes after this many kills
// inside the Den (level 8) on one difficulty.
inline constexpr uint32_t DenLevel = 8;
inline constexpr uint32_t DenKills = 20;

inline constexpr auto Find(Trigger trigger, uint32_t id) -> std::optional<uint8_t> {
	for (const auto& entry : Triggers) {
		if (entry.trigger == trigger && entry.id == id) {
			return entry.flag;
		}
	}
	return std::nullopt;
}

// The Scroll of Inifuss, and what Akara turns it into (the Key to the Cairn Stones),
// which counts as finding it too. Tristram can't be reached without it, so The Search
// for Cain finds it as well (#123).
inline constexpr uint32_t InifussScroll     = Code("bks");
inline constexpr uint32_t DecipheredScroll  = Code("bkd");
inline constexpr uint8_t  SearchForCainFlag = 4;

// The data::QuestItems index of a quest item code.
inline constexpr auto QuestItemIndex(uint32_t code) -> std::optional<uint32_t> {
	if (code == DecipheredScroll) {
		code = InifussScroll;
	}
	for (uint32_t index = 0; index < std::size(data::QuestItems); ++index) {
		const char* text = data::QuestItems[index].code;
		if (code == D2RL::Items::MakeItemCode(text[0], text[1], text[2])) {
			return index;
		}
	}
	return std::nullopt;
}

} // namespace ap::quests
