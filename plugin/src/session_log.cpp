#include "session_log.h"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <mutex>
#include <share.h>
#include <system_error>

namespace ap::session_log {

namespace {

constexpr std::string_view Prefix = "session-";
constexpr std::string_view Suffix = ".log";

std::mutex gMutex;
FILE*      gFile = nullptr;

auto LocalTime(std::chrono::system_clock::time_point now) -> std::tm {
	const std::time_t seconds = std::chrono::system_clock::to_time_t(now);
	std::tm local {};
	localtime_s(&local, &seconds);
	return local;
}

auto Milliseconds(std::chrono::system_clock::time_point now) -> int {
	return static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000);
}

auto IsSessionFile(const std::filesystem::path& path) -> bool {
	const std::string name = path.filename().string();
	return name.starts_with(Prefix) && name.ends_with(Suffix);
}

// Whether a session file has a line saying the session ended normally.
auto EndedCleanly(const std::filesystem::path& path) -> bool {
	std::ifstream in(path, std::ios::binary);
	std::string line;
	while (std::getline(in, line)) {
		if (!line.empty() && line.back() == '\r') {
			line.pop_back();
		}
		if (line.ends_with(EndLine) || line.ends_with(ExitLine)) {
			return true;
		}
	}
	return false;
}

} // namespace

auto FileName(std::chrono::system_clock::time_point now) -> std::string {
	const std::tm local = LocalTime(now);
	char name[64] {};
	std::snprintf(name, sizeof(name), "session-%04d-%02d-%02d_%02d-%02d-%02d-%03d.log", local.tm_year + 1900, local.tm_mon + 1, local.tm_mday,
		local.tm_hour, local.tm_min, local.tm_sec, Milliseconds(now));
	return name;
}

auto Stale(std::vector<std::filesystem::path> files, size_t keep) -> std::vector<std::filesystem::path> {
	std::erase_if(files, [](const auto& file) { return !IsSessionFile(file); });
	std::ranges::sort(files, [](const auto& a, const auto& b) { return a.filename() < b.filename(); });
	if (files.size() <= keep) {
		return {};
	}
	files.resize(files.size() - keep);
	return files;
}

auto Open(const std::filesystem::path& folder, size_t keep, std::chrono::system_clock::time_point now) -> Opened {
	Opened opened;
	std::error_code error;
	std::filesystem::create_directories(folder, error);
	std::vector<std::filesystem::path> files;
	for (const auto& entry : std::filesystem::directory_iterator(folder, error)) {
		if (entry.is_regular_file(error) && IsSessionFile(entry.path())) {
			files.push_back(entry.path());
		}
	}
	if (!files.empty()) {
		opened.previous = *std::ranges::max_element(files, [](const auto& a, const auto& b) { return a.filename() < b.filename(); });
		opened.previousUnclean = !EndedCleanly(opened.previous);
	}
	// The new file counts toward keep.
	for (const auto& file : Stale(files, keep > 0 ? keep - 1 : 0)) {
		std::filesystem::remove(file, error);
	}
	const auto path = folder / FileName(now);
	// Shared, so the log can be read while the game runs.
	FILE* file = _wfsopen(path.c_str(), L"ab", _SH_DENYNO);
	if (file == nullptr) {
		return opened;
	}
	std::lock_guard lock(gMutex);
	if (gFile != nullptr) {
		std::fclose(gFile);
	}
	gFile = file;
	opened.path = path;
	return opened;
}

void Write(std::string_view level, std::string_view text) {
	const auto now = std::chrono::system_clock::now();
	const std::tm local = LocalTime(now);
	char stamp[32] {};
	std::snprintf(stamp, sizeof(stamp), "[%02d:%02d:%02d.%03d] ", local.tm_hour, local.tm_min, local.tm_sec, Milliseconds(now));
	std::lock_guard lock(gMutex);
	if (gFile == nullptr) {
		return;
	}
	std::fputs(stamp, gFile);
	std::fwrite(level.data(), 1, level.size(), gFile);
	std::fputs(" ", gFile);
	std::fwrite(text.data(), 1, text.size(), gFile);
	std::fputs("\r\n", gFile);
	std::fflush(gFile);
}

void Close() {
	Write("INFO", EndLine);
	std::lock_guard lock(gMutex);
	if (gFile != nullptr) {
		std::fclose(gFile);
		gFile = nullptr;
	}
}

} // namespace ap::session_log
