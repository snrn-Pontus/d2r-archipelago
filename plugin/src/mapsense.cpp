#include "mapsense.h"

#include "ids.h"

#include <algorithm>
#include <iterator>

namespace ap::mapsense {

namespace {

constexpr const char* ActNames[]        = { "Act I", "Act II", "Act III", "Act IV", "Act V" };
constexpr const char* DifficultyNames[] = { "Normal", "Nightmare", "Hell" };

}

auto Destinations(const std::set<int64_t>& sideAreas) -> std::vector<Destination> {
	std::vector<Destination> destinations;
	for (const auto& area : data::SideAreas) {
		Destination destination { &area, {} };
		bool any = false;
		for (uint32_t difficulty = 0; difficulty < 3; ++difficulty) {
			if (sideAreas.contains(ids::AreaLocation(difficulty, area.id))) {
				destination.difficulties[difficulty] = true;
				any = true;
			}
		}
		if (any) {
			destinations.push_back(destination);
		}
	}
	return destinations;
}

auto CommentText(std::string_view text) -> std::string {
	std::string comment(text);
	for (char& c : comment) {
		if (static_cast<unsigned char>(c) < 0x20 || c == 0x7F) {
			c = ' ';
		}
	}
	return comment;
}

auto FormatTargets(const std::vector<Destination>& destinations, std::string_view seed, std::string_view slot) -> std::string {
	std::string text = "# Diablo II: Resurrected Archipelago side area checks for " + CommentText(slot) + ", seed " + CommentText(seed) + ".\n"
	                 + "# Written by d2r-ap-setup --mapsense for RuffnecKk MapSense " + Version + " (config schema_version "
	                 + std::to_string(SchemaVersion) + ").\n"
	                 + "#\n"
	                 + "# In MapSense's ruffneckk-mapsense.toml, under [navigation.custom_levels], replace targets = [ ... ] with\n"
	                 + "# the list below (or copy its lines into your own list), turn on Custom Levels in the MapSense menu and\n"
	                 + "# restart D2R: MapSense only reads the list when the game starts.\n"
	                 + "#\n"
	                 + "# These are areas, not checks. The list doesn't change as you play, so an area stays on it after its\n"
	                 + "# check is sent; ap-missing in game shows what's left. MapSense draws a line when a destination\n"
	                 + "# connects to the area you're in, so for a deeper level the line shows from the level above.\n"
	                 + "# MapSense's other features (revealing maps, monsters) work on their own settings, not this list.\n";
	if (destinations.empty()) {
		return text + "# This seed has no side area checks.\ntargets = []\n";
	}
	text += "targets = [\n";
	for (const auto& destination : destinations) {
		std::string entry = "  { level_id = " + std::to_string(destination.area->id) + " },";
		entry.resize(std::max<size_t>(entry.size() + 1, 24), ' ');
		std::string difficulties;
		for (size_t difficulty = 0; difficulty < destination.difficulties.size(); ++difficulty) {
			if (destination.difficulties[difficulty]) {
				difficulties += (difficulties.empty() ? "" : ", ") + std::string(DifficultyNames[difficulty]);
			}
		}
		const char* act = destination.area->act < std::size(ActNames) ? ActNames[destination.area->act] : "?";
		text += entry + "# " + CommentText(destination.area->name) + " (" + act + "; " + difficulties + ")\n";
	}
	return text + "]\n";
}

}
