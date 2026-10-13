#pragma once

#include <chrono>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

// The plugin's own log: one file per launch in d2rloader\archipelago\logs,
// the newest few kept. D2RLoader's log is wiped on every launch, so after a
// crash it no longer shows what led up to it; this one still does. Every
// line is flushed as it's written, so a crash loses nothing.

namespace ap::session_log {

// Under the game folder.
inline constexpr const wchar_t* Folder = L"d2rloader\\archipelago\\logs";
inline constexpr size_t         Keep   = 10;
// The last line of a session that closed normally, and the plugin's line for
// D2R starting to quit (in case D2RLoader doesn't unload plugins on exit).
inline constexpr std::string_view EndLine  = "session end";
inline constexpr std::string_view ExitLine = "D2R is closing.";

struct Opened {
	std::filesystem::path path;
	// The previous session's file, and whether it has neither EndLine nor ExitLine (a crash or a kill).
	std::filesystem::path previous;
	bool                  previousUnclean = false;
};

// Starts a new session file in folder and deletes the oldest past keep. Empty
// path if the file couldn't be made (logging then only goes to D2RLoader).
auto Open(const std::filesystem::path& folder, size_t keep, std::chrono::system_clock::time_point now) -> Opened;

// A line with the time and level ("INFO", "WARN", "ERROR"). Thread-safe;
// does nothing before Open or after Close.
void Write(std::string_view level, std::string_view text);

// Writes EndLine and closes the file.
void Close();

// The session file name for a time: sorts in time order.
auto FileName(std::chrono::system_clock::time_point now) -> std::string;

// Of the session files in a folder, the ones to delete so keep remain (oldest first).
auto Stale(std::vector<std::filesystem::path> files, size_t keep) -> std::vector<std::filesystem::path>;

} // namespace ap::session_log
