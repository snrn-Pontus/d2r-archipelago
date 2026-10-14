#pragma once

#include "loot.h"

#include <cstdint>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// What the Potion lock does with potions you pick up while it's on (the seed's
// potion_lock_mode): set them aside until Potion Access, or drink them at once.

namespace ap::potion_lock {

enum class Mode { SetAside, Drink };

// The slot data's potion_lock_mode key; anything else keeps them set aside.
inline auto ModeFromKey(std::string_view key) -> Mode {
	return key == "drink" ? Mode::Drink : Mode::SetAside;
}

inline auto IsPotion(uint32_t code) -> bool {
	static const std::set<uint32_t> potions = [] {
		std::set<uint32_t> codes;
		for (const char* text : { "hp1", "hp2", "hp3", "hp4", "hp5", "mp1", "mp2", "mp3", "mp4", "mp5", "rvs", "rvl", "yps", "vps", "wms" }) {
			codes.insert(loot::CodeFromText(text));
		}
		return codes;
	}();
	return potions.contains(code);
}

// What drinking a potion restores. D2RCore can fill life and mana but can't
// add a set amount (the plugin can't read the current values), so any healing
// potion fills life and any mana potion fills mana.
struct Effect {
	bool life = false;
	bool mana = false;
};

// The effect of drinking this potion, or none for the ones the plugin can't
// apply (stamina, antidote and thawing potions), which stay set aside.
inline auto DrinkEffect(uint32_t code) -> std::optional<Effect> {
	for (const char* text : { "hp1", "hp2", "hp3", "hp4", "hp5" }) {
		if (code == loot::CodeFromText(text)) {
			return Effect { .life = true };
		}
	}
	for (const char* text : { "mp1", "mp2", "mp3", "mp4", "mp5" }) {
		if (code == loot::CodeFromText(text)) {
			return Effect { .mana = true };
		}
	}
	if (code == loot::CodeFromText("rvs") || code == loot::CodeFromText("rvl")) {
		return Effect { .life = true, .mana = true };
	}
	return std::nullopt;
}

// The D2RCore console commands that drink these potions together: heal fills
// life, and setmana clamps to the maximum, so a large amount fills mana. Each
// runs once however many potions want it.
inline auto DrinkCommands(std::span<const Effect> effects) -> std::vector<std::string> {
	bool life = false, mana = false;
	for (const Effect& effect : effects) {
		life = life || effect.life;
		mana = mana || effect.mana;
	}
	std::vector<std::string> commands;
	if (life) {
		commands.emplace_back("heal");
	}
	if (mana) {
		commands.emplace_back("setmana 100000");
	}
	return commands;
}

} // namespace ap::potion_lock
