#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ap::panel {

// The in-game panels: grids of buttons opened with a key or a console command.
// The command panel (F9 or ap-panel) has the Archipelago commands a player
// needs; the debug panel (F10 or ap-debug, after ap-debug on) has test tools.
// Each button sends a UI message the plugin handles; ids are unique across
// both panels. D2R drops button messages to targets it doesn't know, so they
// go to PanelManager as "PanelManager:OpenPanel:archipelago-run/<id>", which
// the plugin consumes before D2R looks for that panel.

// The click message for a button.
auto ClickMessage(std::string_view id) -> std::string;

// The button id a UI message carries, or empty if it isn't a button click.
auto ClickedButton(std::string_view target, std::string_view command, std::string_view text) -> std::string_view;

struct Button {
	std::string_view id;
	std::string_view label;
	// What it does: a plugin console command ("ap-core goto 3"), or server chat
	// commands ("!hint"), several joined by "; ".
	std::string_view action;
};

// A column of buttons under an optional heading.
struct Column {
	std::string_view heading;
	std::span<const Button> buttons;
};

struct Panel {
	const char* localId;
	const char* name; // "archipelago/" + localId
	const char* path; // the HD layout D2R loads for name
	std::string_view title;
	std::string_view help;
	std::span<const Column> columns;
	// Space between buttons.
	int gap;
};

auto Main() -> const Panel&;
auto Debug() -> const Panel&;
auto Repairs() -> const Panel&;

// The button with this id on the panel, or nullptr.
auto Find(const Panel& panel, std::string_view id) -> const Button*;

// The panel's layout JSON.
auto Layout(const Panel& panel) -> std::string;

// An action's parts: "!a; !b" is two server commands.
auto SplitAction(std::string_view action) -> std::vector<std::string_view>;

} // namespace ap::panel
