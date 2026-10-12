#pragma once

#include "game_data.h"

#include <D2RLPlugin/item.h>

#include <cstdint>
#include <string>

namespace ap {

struct Config {
	std::string server;
	std::string slot;
	std::string password;
	bool autoConnect = true;
	// The debug panel (F10), set by ap-debug on/off.
	bool debugTools = false;
	// The Codex is the "Archipelago Codex" unique small charm that d2r-ap-setup
	// adds to UniqueItems. If D2R refuses it, the plugin makes a Magic small
	// charm with a fixed, level-1 affix instead ("Stout", +1 Defense).
	std::string codexItem = "cm1";
	uint32_t codexUnique = data::CodexUnique;
	// Combined MagicSuffix/MagicPrefix/AutoMagic id; 0 lets D2R roll one.
	uint32_t codexPrefix = data::CodexPrefix;
	// Properties.txt *Id of "oskill" (+N to a skill usable by any class).
	uint32_t oskillProperty = 265;
};

// Reads the flat `key = value` lines of the plugin's TOML file. Tables are
// ignored, so every key must be unique.
auto ParseConfig(const std::string& text) -> Config;
auto FormatConfig(const Config& config) -> std::string;

} // namespace ap
