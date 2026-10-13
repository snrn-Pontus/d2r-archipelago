#include "command_panel.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>

namespace ap::panel {

namespace {

using json = nlohmann::json;

// Keep routine queries separate from maintenance and consequential actions.
constexpr std::array MainLeft {
	Button { "status", "Status", "ap-status" },
	Button { "missing", "Checks Left Here", "ap-missing" },
	Button { "missing-all", "Checks Left by Act", "ap-missing all" },
	Button { "hints", "Hints Found", "ap-hints" },
	Button { "claim", "Claim Character", "ap-claim" },
};

constexpr std::array MainRight {
	Button { "server-hints", "Server Hints", "!hint" },
	Button { "remaining", "Items Left", "!remaining" },
	Button { "players", "Multiworld Status", "!status" },
	Button { "reconnect", "Reconnect", "ap-reconnect" },
};

constexpr std::array MainTools {
	Button { "ledger", "New Ledger", "ap-ledger" },
	Button { "codex", "Rebuild Codex", "ap-codex" },
	Button { "relearn", "Relearn Skills...", "ap-relearn confirm", true },
	Button { "collect", "Collect...", "!collect", true },
	Button { "disconnect", "Disconnect...", "ap-disconnect", true },
};

constexpr std::array MainColumns {
	Column { "This Game", MainLeft },
	Column { "Server", MainRight },
	Column { "Maintenance", MainTools },
};

constexpr std::array ConfirmRelearn { Button { "confirm-relearn", "Relearn Skills", "ap-relearn confirm" } };
constexpr std::array ConfirmCollect { Button { "confirm-collect", "Collect", "!collect" } };
constexpr std::array ConfirmDisconnect { Button { "confirm-disconnect", "Disconnect", "ap-disconnect" } };
constexpr std::array RelearnColumn { Column { "", ConfirmRelearn } };
constexpr std::array CollectColumn { Column { "", ConfirmCollect } };
constexpr std::array DisconnectColumn { Column { "", ConfirmDisconnect } };

const std::array ConfirmationPanels {
	Panel { "ConfirmRelearn", "archipelago/ConfirmRelearn", "data/global/ui/layouts/archipelago/ConfirmRelearnhd.json",
		"Relearn Skills?", "Restore one level to each missing Archipelago tree skill after a respec.", RelearnColumn, 30, true },
	Panel { "ConfirmCollect", "archipelago/ConfirmCollect", "data/global/ui/layouts/archipelago/ConfirmCollecthd.json",
		"Collect Remaining Checks?", "Ask the server to collect this slot's remaining checks. This cannot be undone.", CollectColumn, 30, true },
	Panel { "ConfirmDisconnect", "archipelago/ConfirmDisconnect", "data/global/ui/layouts/archipelago/ConfirmDisconnecthd.json",
		"Disconnect?", "Stop receiving items and sending checks until you reconnect.", DisconnectColumn, 30, true },
};

// The debug panel, for the playtest checklist. Level ids from Levels.txt.
constexpr std::array DebugGoTo {
	Button { "debug-goto-1", "Rogue Encampment", "ap-core goto 1" },
	Button { "debug-goto-3", "Cold Plains", "ap-core goto 3" },
	Button { "debug-goto-4", "Stony Field", "ap-core goto 4" },
	Button { "debug-goto-5", "Dark Wood", "ap-core goto 5" },
	Button { "debug-goto-38", "Tristram", "ap-core goto 38" },
	Button { "debug-goto-39", "Moo Moo Farm", "ap-core goto 39" },
	Button { "debug-goto-40", "Lut Gholein", "ap-core goto 40" },
	Button { "debug-goto-74", "Arcane Sanctuary", "ap-core goto 74" },
	Button { "debug-goto-75", "Kurast Docks", "ap-core goto 75" },
	Button { "debug-goto-103", "Pandemonium Fortress", "ap-core goto 103" },
	Button { "debug-goto-109", "Harrogath", "ap-core goto 109" },
	Button { "debug-goto-111", "Frigid Highlands", "ap-core goto 111" },
};

// Monster spawns are MonStats rows: 156 Andariel, 229 Radament.
constexpr std::array DebugCharacter {
	Button { "debug-god", "God Mode", "ap-core god" },
	Button { "debug-easykill", "One-Hit Kills", "ap-core easykill" },
	Button { "debug-quests", "Complete Quests", "ap-core quests" },
	Button { "debug-level-6", "Level 6", "ap-core level 6" },
	Button { "debug-level-30", "Level 30", "ap-core level 30" },
	Button { "debug-skillpoints", "+5 Skill Points", "ap-core skillpoints 5" },
	Button { "debug-gold", "+50,000 Gold", "ap-core gold 50000" },
	Button { "debug-waypoints", "All Waypoints", "ap-core waypoints" },
	Button { "debug-fallen", "Spawn 25 Fallen", "ap-core mspawn fallen1,25" },
	Button { "debug-andariel", "Spawn Andariel", "ap-core mspawn 156" },
	Button { "debug-radament", "Spawn Radament", "ap-core mspawn 229" },
	Button { "debug-killme", "Kill Me", "ap-core killme" },
};

// Made as if found, so they count for checks.
constexpr std::array DebugItems {
	Button { "debug-item-bks", "Scroll of Inifuss", "ap-debug spawn bks" },
	Button { "debug-item-leg", "Wirt's Leg", "ap-debug spawn leg" },
	Button { "debug-item-ass", "Book of Skill", "ap-debug spawn ass" },
	Button { "debug-item-tr1", "Horadric Scroll", "ap-debug spawn tr1" },
	Button { "debug-item-xyz", "Potion of Life", "ap-debug spawn xyz" },
	Button { "debug-item-tr2", "Scroll of Resistance", "ap-debug spawn tr2" },
	Button { "debug-item-j34", "Jade Figurine", "ap-debug spawn j34" },
	Button { "debug-item-bbb", "Lam Esen's Tome", "ap-debug spawn bbb" },
	Button { "debug-item-ice", "Malah's Potion", "ap-debug spawn ice" },
	Button { "debug-item-unique", "Unique Ring", "ap-debug spawn rin unique" },
	Button { "debug-item-tsc", "Town Portal Scroll", "ap-debug spawn tsc" },
	Button { "debug-item-hp1", "Healing Potion", "ap-debug spawn hp1" },
};

// The server's !getitem, so they arrive like any received item.
constexpr std::array DebugServer {
	Button { "debug-locks", "All Lock Items",
		"!getitem Horadric Cube Access; !getitem Town Portal Access; !getitem Potion Access; !getitem Stash Access; "
		"!getitem Merchant Access; !getitem Act I Waypoints; !getitem Act II Waypoints; !getitem Act III Waypoints" },
	Button { "debug-keys", "Normal Act Keys",
		"!getitem Normal Act II Key; !getitem Normal Act III Key; !getitem Normal Act IV Key; !getitem Normal Act V Key" },
	Button { "debug-rewards", "Rewards",
		"!getitem Experience; !getitem Random Charm; !getitem Random Jewel; !getitem Socketed Base; "
		"!getitem Random Set Item; !getitem Random Unique; !getitem Prophecy" },
	Button { "debug-empowerment", "Codex Empowerment", "!getitem Codex Empowerment" },
	Button { "debug-skill-point", "Skill Point", "!getitem Skill Point" },
	Button { "debug-junk", "Junk Backpack", "!getitem Junk Backpack" },
	Button { "debug-ambush", "Ambush Trap", "!getitem Ambush Trap" },
	Button { "debug-poison", "Poison Cloud Trap", "!getitem Poison Cloud Trap" },
	Button { "debug-firestorm", "Firestorm Trap", "!getitem Firestorm Trap" },
	Button { "debug-mana", "Mana Drain Trap", "!getitem Mana Drain Trap" },
	Button { "debug-tax", "Tax Collector Trap", "!getitem Tax Collector Trap" },
	Button { "debug-merc", "Mercenary Down Trap", "!getitem Mercenary Down Trap" },
};

// A fresh character's kit (made as rewards, no checks), the log tools for bug reports,
// and set-ups for the checklist's harder scenarios: kill counts one short of a check or
// saved past one without it (Reconnect recovers them), saves that fail, potions for the
// Potion lock and a crash.
constexpr std::array DebugTesting {
	Button { "debug-kit", "Starter Kit", "ap-debug kit" },
	Button { "debug-mark", "Mark the Log", "ap-debug mark" },
	Button { "debug-state", "Log State", "ap-debug state" },
	Button { "debug-kills-next", "Kills to Next Check", "ap-debug kills" },
	Button { "debug-kills-260", "Save 260 Kills", "ap-debug kills 260" },
	Button { "debug-cows-next", "Cows to Next Check", "ap-debug cows" },
	Button { "debug-reconnect", "Reconnect", "ap-reconnect" },
	Button { "debug-disconnect", "Disconnect", "ap-disconnect" },
	Button { "debug-savefail-on", "Saves Fail", "ap-debug savefail on" },
	Button { "debug-savefail-off", "Saves Work", "ap-debug savefail off" },
	Button { "debug-potions", "4 Healing Potions", "ap-debug give hp1 4" },
	Button { "debug-crash", "Crash D2R (Twice)", "ap-debug crash" },
};

constexpr std::array DebugColumns {
	Column { "Go To", DebugGoTo },
	Column { "Character", DebugCharacter },
	Column { "Items", DebugItems },
	Column { "From the Server", DebugServer },
	Column { "Testing", DebugTesting },
};

const Panel MainPanel {
	.localId = "CommandPanel",
	.name    = "archipelago/CommandPanel",
	.path    = "data/global/ui/layouts/archipelago/CommandPanelhd.json",
	.title   = "Archipelago",
	.help    = "Choose a command. The panel closes so you can read its answer in chat.",
	.columns = MainColumns,
	.gap     = 30,
};

const Panel DebugPanel {
	.localId = "DebugPanel",
	.name    = "archipelago/DebugPanel",
	.path    = "data/global/ui/layouts/archipelago/DebugPanelhd.json",
	.title   = "Archipelago Debug",
	.help    = "Test tools. Items from the server stay in this seed.",
	.columns = DebugColumns,
	.gap     = 14,
};

constexpr std::array RepairButtons {
	Button { "repair-all", "Repair All (NPC Prices)", "ap-repairs all" },
};
constexpr std::array RepairColumns { Column { "", RepairButtons } };
const Panel RepairPanel {
	.localId = "RepairPanel",
	.name    = "archipelago/RepairPanel",
	.path    = "data/global/ui/layouts/archipelago/RepairPanelhd.json",
	.title   = "Repairs",
	.help    = "Merchant Access is locked. Repairs cost gold.",
	.columns = RepairColumns,
	.gap     = 30,
};

// ModalButton is 531x102.
constexpr int ButtonWidth = 531, ButtonHeight = 102, HeadingHeight = 80;

auto ButtonWidget(std::string_view name, std::string_view label, int x, int y, std::string message) -> json {
	return {
		{ "type", "ButtonWidget" },
		{ "name", name },
		{ "fields",
			{
				{ "anchor", { { "x", 0.5 }, { "y", 0.5 } } },
				{ "rect", { { "x", x }, { "y", y } } },
				{ "filename", "Panel\\Modals\\ModalButton" },
				{ "focusIndicatorFilename", "Controller/HoverImages/ModalButton_Hover" },
				{ "hoveredFrame", 3 },
				{ "textString", label },
				{ "onClickMessage", std::move(message) },
				{ "textColor", "$LightButtonTextColor" },
				{ "text/style", "$StyleFEButtonText" },
				{ "focusOnMouseOver", true },
				{ "sound", "select" },
			} },
	};
}

} // namespace

auto Main() -> const Panel& {
	return MainPanel;
}

auto Debug() -> const Panel& {
	return DebugPanel;
}

auto Repairs() -> const Panel& {
	return RepairPanel;
}

auto Confirmations() -> std::span<const Panel> {
	return ConfirmationPanels;
}

auto ConfirmationFor(std::string_view id) -> const Panel* {
	for (const auto& panel : ConfirmationPanels) {
		if (panel.columns[0].buttons[0].id == "confirm-" + std::string(id)) {
			return &panel;
		}
	}
	return nullptr;
}

auto Find(const Panel& panel, std::string_view id) -> const Button* {
	for (const auto& column : panel.columns) {
		for (const auto& button : column.buttons) {
			if (button.id == id) {
				return &button;
			}
		}
	}
	return nullptr;
}

constexpr std::string_view ClickPrefix = "archipelago-run/";

auto ClickMessage(std::string_view id) -> std::string {
	return "PanelManager:OpenPanel:" + std::string(ClickPrefix) + std::string(id);
}

auto ClickedButton(std::string_view target, std::string_view command, std::string_view text) -> std::string_view {
	// The panel name may arrive in quotes.
	if (text.size() >= 2 && text.front() == '"' && text.back() == '"') {
		text = text.substr(1, text.size() - 2);
	}
	if (target != "PanelManager" || command != "OpenPanel" || !text.starts_with(ClickPrefix)) {
		return {};
	}
	return text.substr(ClickPrefix.size());
}

auto SplitAction(std::string_view action) -> std::vector<std::string_view> {
	std::vector<std::string_view> parts;
	while (!action.empty()) {
		const size_t end = action.find("; ");
		parts.push_back(action.substr(0, end));
		action = end == std::string_view::npos ? std::string_view() : action.substr(end + 2);
	}
	return parts;
}

auto Layout(const Panel& panel) -> std::string {
	const int columns = static_cast<int>(panel.columns.size());
	int rows = 0;
	bool headings = false;
	for (const auto& column : panel.columns) {
		rows = std::max(rows, static_cast<int>(column.buttons.size()));
		headings = headings || !column.heading.empty();
	}
	// Centred on the screen: the headings, the grid, then the Close button under it.
	const int pitch = ButtonHeight + panel.gap;
	const int width = columns * ButtonWidth + (columns - 1) * panel.gap;
	const bool confirmation = panel.confirmation;
	const int heading = headings ? HeadingHeight : 0;
	const int height = heading + (rows + 1) * pitch - panel.gap;
	const int left = -width / 2, top = -height / 2;
	const int grid = top + heading;

	json children = json::array();
	// Dims the game and catches clicks, like the pause menu.
	children.push_back({
		{ "type", "RectangleWidget" },
		{ "name", "Backdrop" },
		{ "fields", { { "fitToScreen", true }, { "color", { 0.0, 0.0, 0.0, 0.6 } } } },
		{ "children", json::array({ { { "type", "ClickCatcherWidget" }, { "name", "Catcher" }, { "fields", { { "fitToParent", true } } } } }) },
	});
	children.push_back({
		{ "type", "TextBoxWidget" },
		{ "name", "Title" },
		{ "fields",
			{
				{ "anchor", { { "x", 0.5 }, { "y", 0.5 } } },
				{ "rect", { { "x", confirmation ? -800 : left }, { "y", top - 230 }, { "width", confirmation ? 1600 : width }, { "height", 110 } } },
				{ "text", panel.title },
				{ "style", { { "fontColor", "$PanelTitleColor" }, { "pointSize", "$XExtraLargeFontSize" }, { "alignment", { { "h", "center" }, { "v", "center" } } } } },
			} },
	});
	children.push_back({
		{ "type", "TextBoxWidget" },
		{ "name", "Help" },
		{ "fields",
			{
				{ "anchor", { { "x", 0.5 }, { "y", 0.5 } } },
				{ "rect", { { "x", confirmation ? -800 : left }, { "y", top - 110 }, { "width", confirmation ? 1600 : width }, { "height", 90 } } },
				{ "text", panel.help },
				{ "style", "$StyleModalDialogDescription" },
			} },
	});
	for (int c = 0; c < columns; ++c) {
		const auto& column = panel.columns[static_cast<size_t>(c)];
		const int x = left + c * (ButtonWidth + panel.gap);
		if (!column.heading.empty()) {
			children.push_back({
				{ "type", "TextBoxWidget" },
				{ "name", "Heading" + std::to_string(c) },
				{ "fields",
					{
						{ "anchor", { { "x", 0.5 }, { "y", 0.5 } } },
						{ "rect", { { "x", x }, { "y", top }, { "width", ButtonWidth }, { "height", HeadingHeight - 10 } } },
						{ "text", column.heading },
						{ "style", { { "fontColor", "$PanelTitleColor" }, { "alignment", { { "h", "center" }, { "v", "center" } } } } },
					} },
			});
		}
		for (size_t row = 0; row < column.buttons.size(); ++row) {
			const auto& button = column.buttons[row];
			children.push_back(ButtonWidget(button.id, button.label, x, grid + static_cast<int>(row) * pitch,
				ClickMessage(button.id)));
		}
	}
	children.push_back(ButtonWidget("Close", confirmation ? "Cancel" : "Close", -ButtonWidth / 2, grid + rows * pitch, std::string("PanelManager:ClosePanel:") + panel.name));

	const json root {
		{ "type", "Panel" },
		{ "name", panel.name },
		{ "fields", { { "fitToParent", true }, { "priority", 8500 }, { "defaultWidget", confirmation ? std::string_view("Close") : panel.columns[0].buttons[0].id } } },
		{ "children", std::move(children) },
	};
	return root.dump(1, '\t');
}

} // namespace ap::panel
