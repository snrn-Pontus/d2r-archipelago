#include "config.h"

#include <sstream>

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

auto ParseConfig(const std::string& text) -> Config {
	Config config;
	std::istringstream lines(text);
	std::string line;
	while (std::getline(lines, line)) {
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
		} else if (key == "codex_item" && !value.empty()) {
			config.codexItem = value;
		} else if (key == "codex_unique" && !value.empty()) {
			config.codexUnique = static_cast<uint32_t>(std::stoul(value));
		} else if (key == "codex_prefix" && !value.empty()) {
			config.codexPrefix = static_cast<uint32_t>(std::stoul(value));
		} else if (key == "oskill_property" && !value.empty()) {
			config.oskillProperty = static_cast<uint32_t>(std::stoul(value));
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
	    << "debug_tools = " << (config.debugTools ? "true" : "false") << "\n\n"
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

} // namespace ap
