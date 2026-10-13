#include "config.h"

#include <algorithm>
#include <charconv>
#include <sstream>
#include <string_view>

namespace ap {

namespace {

auto Trim(const std::string& text) -> std::string {
	const auto first = text.find_first_not_of(" \t\r");
	const auto last = text.find_last_not_of(" \t\r");
	return first == std::string::npos ? std::string() : text.substr(first, last - first + 1);
}

// Unquotes a TOML basic string, handling \" and \\ escapes. Bare values
// (numbers, booleans) come back trimmed, with any trailing comment removed.
auto Value(const std::string& raw) -> std::string {
	const std::string text = Trim(raw);
	if (text.empty() || text.front() != '"') {
		return Trim(text.substr(0, text.find('#')));
	}
	std::string value;
	for (size_t i = 1; i < text.size() && text[i] != '"'; ++i) {
		if (text[i] == '\\' && i + 1 < text.size()) {
			++i;
		}
		value += text[i];
	}
	return value;
}

auto Quote(const std::string& text) -> std::string {
	std::string quoted = "\"";
	for (const char c : text) {
		if (c == '"' || c == '\\') {
			quoted += '\\';
		}
		quoted += c;
	}
	return quoted + "\"";
}

} // namespace

auto OtherSendsName(OtherSends setting) -> const char* {
	return setting == OtherSends::Mine ? "mine" : setting == OtherSends::All ? "all" : "progression";
}

auto ParseConfig(const std::string& text, std::vector<std::string>* warnings) -> Config {
	Config config;
	std::istringstream lines(text);
	std::string line;
	size_t lineNumber = 0;
	while (std::getline(lines, line)) {
		++lineNumber;
		const std::string trimmed = Trim(line);
		const auto equals = trimmed.find('=');
		if (trimmed.empty() || trimmed.front() == '#' || trimmed.front() == '[' || equals == std::string::npos) {
			continue;
		}
		const std::string key = Trim(trimmed.substr(0, equals));
		const std::string value = Value(trimmed.substr(equals + 1));
		if (key == "server") {
			config.server = value;
		} else if (key == "slot") {
			config.slot = value;
		} else if (key == "password") {
			config.password = value;
		} else if (key == "auto_connect") {
			config.autoConnect = value == "true";
		} else if (key == "debug_tools") {
			config.debugTools = value == "true";
		} else if (key == "repair_only") {
			config.repairOnly = value == "true";
		} else if (key == "notifications") {
			config.notifications = value != "false";
		} else if (key == "banner_sounds") {
			config.bannerSounds = value != "false";
		} else if (key == "banner_volume" && !value.empty()) {
			// No exceptions: they'd escape the noexcept plugin load. Junk keeps the default.
			long long volume = 0;
			const char* last = value.data() + value.size();
			if (const auto [end, error] = std::from_chars(value.data(), last, volume); error == std::errc {} && end == last) {
				config.bannerVolume = static_cast<int>(std::clamp(volume, 0LL, 200LL));
			}
		} else if (key == "status_line") {
			config.statusLine = value != "false";
		} else if (key == "status_position" && !value.empty()) {
			config.statusPosition = value;
		} else if (key == "other_sends") {
			if (value == "mine" || value == "progression" || value == "all") {
				config.otherSends = value == "mine" ? OtherSends::Mine : value == "all" ? OtherSends::All : OtherSends::Progression;
			} else if (warnings != nullptr) {
				warnings->push_back("Archipelago: invalid other_sends on config line " + std::to_string(lineNumber)
				    + "; expected mine, progression or all, keeping " + OtherSendsName(config.otherSends) + ".");
			}
		} else if (key == "codex_item" && !value.empty()) {
			config.codexItem = value;
		} else if (key == "codex_unique" || key == "codex_prefix" || key == "oskill_property") {
			// Parse directly into the destination type to reject overflow as well
			// as negative IDs and trailing junk, without throwing at plugin load.
			uint32_t id = 0;
			// TOML allows an explicit positive sign, which from_chars skips
			// only when we remove it ourselves. A negative sign stays invalid.
			const char* first = value.data() + (!value.empty() && value.front() == '+' ? 1 : 0);
			const char* last = value.data() + value.size();
			const auto [end, error] = std::from_chars(first, last, id);
			uint32_t& setting = key == "codex_unique" ? config.codexUnique
			    : key == "codex_prefix" ? config.codexPrefix : config.oskillProperty;
			if (error == std::errc {} && end == last) {
				setting = id;
			} else if (warnings != nullptr) {
				warnings->push_back("Archipelago: invalid " + key + " on config line " + std::to_string(lineNumber)
				    + "; expected an unsigned 32-bit integer, keeping " + std::to_string(setting) + ".");
			}
		}
	}
	return config;
}

auto FormatConfig(const Config& config) -> std::string {
	std::ostringstream out;
	out << "[archipelago]\n"
	    << "# Set by ap-connect. The plugin reconnects on its own when auto_connect is true.\n"
	    << "server = " << Quote(config.server) << "\n"
	    << "slot = " << Quote(config.slot) << "\n"
	    << "password = " << Quote(config.password) << "\n"
	    << "auto_connect = " << (config.autoConnect ? "true" : "false") << "\n"
	    << "# Set by ap-debug on/off: the debug panel (F10) for testing.\n"
	    << "debug_tools = " << (config.debugTools ? "true" : "false") << "\n"
	    << "# Experimental repairs while Merchant Access is locked; set with ap-repairs on/off.\n"
	    << "repair_only = " << (config.repairOnly ? "true" : "false") << "\n"
	    << "# On-screen banners for items received and checks sent.\n"
	    << "notifications = " << (config.notifications ? "true" : "false") << "\n"
	    << "# Game sounds with the gold, red and white banners.\n"
	    << "banner_sounds = " << (config.bannerSounds ? "true" : "false") << "\n"
	    << "# Their volume in percent, 0 to 200 (the game's own loudness). Low by default, since they ignore the game's volume and are jarring louder.\n"
	    << "banner_volume = " << config.bannerVolume << "\n"
	    << "# The always-on checks line; status_position is top-left, top, top-right, bottom-left or bottom-right.\n"
	    << "status_line = " << (config.statusLine ? "true" : "false") << "\n"
	    << "status_position = " << Quote(config.statusPosition) << "\n"
	    << "# Items other players send each other, shown greyed in the chat: mine (none of them), progression or all.\n"
	    << "other_sends = " << Quote(OtherSendsName(config.otherSends)) << "\n\n"
	    << "# The Codex charm that carries your received skills.\n"
	    << "codex_item = " << Quote(config.codexItem) << "\n"
	    << "# UniqueItems record of the Archipelago Codex, added by d2r-ap-setup.\n"
	    << "codex_unique = " << config.codexUnique << "\n"
	    << "# Affix forced onto the fallback Magic Codex (combined affix id); 0 lets D2R roll one.\n"
	    << "codex_prefix = " << config.codexPrefix << "\n"
	    << "# Properties.txt *Id of oskill.\n"
	    << "oskill_property = " << config.oskillProperty << "\n";
	return out.str();
}

auto FormatLoaderSettings(const std::vector<std::pair<std::string, std::string>>& settings) -> std::string {
	const auto name = [](const std::string& text) {
		return !text.empty() && std::ranges::all_of(text, [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'; });
	};
	const auto value = [](const std::string& text) {
		const size_t digits = !text.empty() && text.front() == '-' ? 1 : 0;
		return text == "true" || text == "false"
		    || (text.size() > digits && std::ranges::all_of(text.substr(digits), [](char c) { return c >= '0' && c <= '9'; }));
	};
	// Table -> its keys, both in the order they first appear.
	std::vector<std::pair<std::string, std::vector<std::string>>> tables;
	constexpr std::string_view Prefix = "d2rcore.";
	for (const auto& [path, setting] : settings) {
		const auto dot = path.find('.', Prefix.size());
		if (!path.starts_with(Prefix) || dot == std::string::npos || !value(setting)) {
			continue;
		}
		const std::string table = path.substr(Prefix.size(), dot - Prefix.size());
		const std::string key = path.substr(dot + 1);
		if (!name(table) || !name(key)) {
			continue;
		}
		auto found = std::ranges::find(tables, table, &std::pair<std::string, std::vector<std::string>>::first);
		if (found == tables.end()) {
			tables.push_back({ table, {} });
			found = tables.end() - 1;
		}
		found->second.push_back(key + " = " + setting);
	}
	std::ostringstream out;
	out << "# Written by d2r-ap-setup from your Archipelago options (the D2RLoader Settings group), each time it\n"
	    << "# sets up a seed. These override your own D2RLoader settings while the Archipelago mod runs; change\n"
	    << "# them in your options YAML, since edits here are replaced.\n";
	for (const auto& [table, lines] : tables) {
		out << "\n[d2rcore." << table << "]\n";
		for (const auto& line : lines) {
			out << line << "\n";
		}
	}
	return out.str();
}

} // namespace ap
