#pragma once

// The seed's side areas as destinations for RuffnecKk MapSense, a separate
// D2RLoader plugin that draws a line to chosen areas on the automap. The setup
// tool writes them with --mapsense for the player to paste into MapSense's own
// config; nothing here touches MapSense or its files.

#include "game_data.h"

#include <array>
#include <cstdint>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace ap::mapsense {

// The MapSense this was written against: its custom_levels.targets list takes
// { level_id = N } entries and is only read when D2R starts.
inline constexpr const char* Version       = "2.2.0";
inline constexpr int         SchemaVersion = 21;

// A side area and the difficulties on which it's a check.
struct Destination {
	const data::Area*   area;
	std::array<bool, 3> difficulties;
};

// The side areas among the slot data's side_areas location ids, in the order
// of data::SideAreas, each once whatever its difficulties. Other ids are skipped.
auto Destinations(const std::set<int64_t>& sideAreas) -> std::vector<Destination>;

// Text that can go in a TOML comment: control characters (a line break would
// end the comment) become spaces.
auto CommentText(std::string_view text) -> std::string;

// A targets = [ ... ] list for MapSense's [navigation.custom_levels], one
// { level_id = N } per destination with its name, act and difficulties in a
// comment, after a header saying where it goes and what it can't do.
auto FormatTargets(const std::vector<Destination>& destinations, std::string_view seed, std::string_view slot) -> std::string;

}
