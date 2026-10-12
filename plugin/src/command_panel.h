#pragma once

#include <span>
#include <string>
#include <string_view>

namespace ap::panel {

// The in-game command panel: a grid of buttons for the Archipelago commands a
// player needs, opened with a key (F9 by default) or ap-panel. Each button
// sends the UI message "archipelago:Run:<id>", which the plugin handles.

inline constexpr const char* LocalId = "CommandPanel";
inline constexpr const char* Name    = "archipelago/CommandPanel";
inline constexpr const char* Path    = "data/global/ui/layouts/archipelago/CommandPanelhd.json";
inline constexpr const char* Target  = "archipelago";
inline constexpr const char* Command = "Run";

struct Button {
	std::string_view id;
	std::string_view label;
	// What it does: a plugin console command, or a server chat command ("!hint").
	std::string_view action;
};

// Two columns, filled row by row: game-side tools left, the server's right.
auto Buttons() -> std::span<const Button>;

// The button with this id, or nullptr.
auto Find(std::string_view id) -> const Button*;

// The panel's layout JSON (the HD layout D2R loads for Name).
auto Layout() -> std::string;

} // namespace ap::panel
