#pragma once

#include "game_data.h"

#include <D2RLPlugin/item.h>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace ap {

// Which items other players send each other show in the chat: none (only ours),
// their progression items, or all of them.
enum class OtherSends { Mine, Progression, All };

auto OtherSendsName(OtherSends setting) -> const char*;

struct Config {
	std::string server;
	std::string slot;
	std::string password;
	bool autoConnect = true;
	// The debug panel (F10), set by ap-debug on/off.
	bool debugTools = false;
	// Experimental: route a blocked vendor interaction to the native RepairAll
	// action through a separate panel. The stock shop remains blocked.
	bool repairOnly = false;
	// On-screen banners for items received and checks sent.
	bool notifications = true;
	// Game sounds with the gold, red and white banners.
	bool bannerSounds = true;
	// Their loudness in percent of the d2r-ap-setup files (half the game's): 200
	// is the game's own, 0 silent. Low by default: they play over the game's own
	// volume setting, and much louder they're jarring.
	int bannerVolume = 10;
	// The banner: how many lines it shows at once (more wait their turn), and
	// how long a line stays in milliseconds, for white and grey lines and for
	// gold and red ones.
	int bannerLines = 5;
	int bannerMs = 4500;
	int bannerImportantMs = 6000;
	// More than this many checks sent, items received, or ordinary items among
	// them (not keys, locks, skills or traps) at once become one summing line.
	int summarizeChecksOver = 4;
	int summarizeItemsOver = 10;
	int summarizeLootOver = 3;
	// The always-on checks line, and its corner: top-left, top, top-right,
	// bottom-left or bottom-right.
	bool statusLine = true;
	std::string statusPosition = "top-left";
	// Other players' sends to each other in the chat (mine, progression or all).
	OtherSends otherSends = OtherSends::Progression;
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
// ignored, so every key must be unique. Invalid numeric IDs keep their default
// (or previous valid value); optional warnings identify the setting and line.
auto ParseConfig(const std::string& text, std::vector<std::string>* warnings = nullptr) -> Config;
auto FormatConfig(const Config& config) -> std::string;

// The mod's d2rloader.toml: D2RLoader settings the mod overrides, from
// ("d2rcore.<table>.<key>", TOML value) pairs, grouped into their tables.
// Paths outside d2rcore (the global loader settings mods can't override) or
// with odd characters are skipped.
auto FormatLoaderSettings(const std::vector<std::pair<std::string, std::string>>& settings) -> std::string;

} // namespace ap
