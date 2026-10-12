#include "command_panel.h"

#include <nlohmann/json.hpp>

#include <array>

namespace ap::panel {

namespace {

using json = nlohmann::json;

constexpr std::array ButtonList {
	Button { "status", "Status", "ap-status" },
	Button { "server-hints", "Server Hints", "!hint" },
	Button { "missing", "Checks Left Here", "ap-missing" },
	Button { "remaining", "Items Left", "!remaining" },
	Button { "missing-all", "Checks Left by Act", "ap-missing all" },
	Button { "players", "Multiworld Status", "!status" },
	Button { "hints", "Hints Found", "ap-hints" },
	Button { "collect", "Collect", "!collect" },
	Button { "ledger", "New Ledger", "ap-ledger" },
	Button { "reconnect", "Reconnect", "ap-reconnect" },
	Button { "codex", "Rebuild Codex", "ap-codex" },
	Button { "disconnect", "Disconnect", "ap-disconnect" },
};

constexpr int Columns = 2;
// ModalButton is 531x102.
constexpr int ButtonWidth = 531, ButtonHeight = 102, Gap = 30;
constexpr int Pitch = ButtonHeight + Gap;

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

auto Buttons() -> std::span<const Button> {
	return ButtonList;
}

auto Find(std::string_view id) -> const Button* {
	for (const auto& button : ButtonList) {
		if (button.id == id) {
			return &button;
		}
	}
	return nullptr;
}

auto Layout() -> std::string {
	constexpr int rows = static_cast<int>((ButtonList.size() + Columns - 1) / Columns);
	// Centred on the screen: the grid, then the Close button under it.
	constexpr int width = Columns * ButtonWidth + (Columns - 1) * Gap;
	constexpr int height = (rows + 1) * Pitch - Gap;
	constexpr int left = -width / 2, top = -height / 2;

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
				{ "rect", { { "x", left }, { "y", top - 230 }, { "width", width }, { "height", 110 } } },
				{ "text", "Archipelago" },
				{ "style", { { "fontColor", "$PanelTitleColor" }, { "pointSize", "$XExtraLargeFontSize" }, { "alignment", { { "h", "center" }, { "v", "center" } } } } },
			} },
	});
	children.push_back({
		{ "type", "TextBoxWidget" },
		{ "name", "Help" },
		{ "fields",
			{
				{ "anchor", { { "x", 0.5 }, { "y", 0.5 } } },
				{ "rect", { { "x", left }, { "y", top - 110 }, { "width", width }, { "height", 70 } } },
				{ "text", "Answers show in the chat." },
				{ "style", "$StyleModalDialogDescription" },
			} },
	});
	for (size_t i = 0; i < ButtonList.size(); ++i) {
		const auto& button = ButtonList[i];
		const int column = static_cast<int>(i % Columns), row = static_cast<int>(i / Columns);
		children.push_back(ButtonWidget(button.id, button.label, left + column * (ButtonWidth + Gap), top + row * Pitch,
			std::string(Target) + ":" + Command + ":" + std::string(button.id)));
	}
	children.push_back(ButtonWidget("Close", "Close", -ButtonWidth / 2, top + rows * Pitch, std::string("PanelManager:ClosePanel:") + Name));

	const json root {
		{ "type", "Panel" },
		{ "name", Name },
		{ "fields", { { "fitToParent", true }, { "priority", 8500 }, { "defaultWidget", ButtonList[0].id } } },
		{ "children", std::move(children) },
	};
	return root.dump(1, '\t');
}

} // namespace ap::panel
