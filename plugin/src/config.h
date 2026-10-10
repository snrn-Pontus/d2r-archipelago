#pragma once

#include <D2RLPlugin/item.h>

#include <cstdint>
#include <string>

namespace ap {

struct Config {
	std::string server;
	std::string slot;
	std::string password;
	bool autoConnect = true;
	// The Codex is a small charm by default; Magic quality so charm stats apply.
	std::string codexItem = "cm1";
	D2RL::Items::Quality codexQuality = D2RL::Items::Quality::Magic;
	// Properties.txt *Id of "oskill" (+N to a skill usable by any class).
	uint32_t oskillProperty = 265;
};

// Reads the flat `key = value` lines of the plugin's TOML file. Tables are
// ignored, so every key must be unique.
auto ParseConfig(const std::string& text) -> Config;
auto FormatConfig(const Config& config) -> std::string;

} // namespace ap
