// Writes the "Archipelago" D2R mod from the player's own game files: class
// skill trees locked (skills only come from the Codex charm), plus the seed's
// randomization (monsters, superuniques, runewords, experience, requirements).
//
//   d2r-ap-setup                                   use the server and slot saved by the plugin
//   d2r-ap-setup --connect <server> <slot> [--password <pw>]
//   d2r-ap-setup --offline                         skill lock only, no seed
//   d2r-ap-setup --extract <folder>                copy the raw tables for tools/gen_data.py
//   d2r-ap-setup --dump <game path> <file>         copy one game file, e.g. data\hd\items\items.json
//   d2r-ap-setup --sounds                          only write the banner sounds
//   d2r-ap-setup --find <text>                     list the game files whose names contain the text
//
// Options for all of them: --game <D2R folder>, --mod <name>, --out <mods folder>.
// Writing the mod needs D2RLoader in the game folder; it installs the plugin
// (d2rl-archipelago.dll next to this tool, or --plugin <file>) into the mod,
// and saves the server and slot so the plugin connects on its own. --play then
// starts the game with the mod (the Archipelago Launcher's button uses it).

#include "ap_client.h"
#include "config.h"
#include "game_data.h"
#include "randomizer.h"
#include "sounds.h"
#include "sprite.h"
#include "table.h"

#include <CascLib.h>
#include <nlohmann/json.hpp>

#include <windows.h>
#include <shellapi.h>
#include <winhttp.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>

#include <chrono>
#include <cstring>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

namespace fs = std::filesystem;
using ap::Table;
namespace rnd = ap::randomizer;

// The tables tools/gen_data.py reads, from the RotW bank.
const char* const ExtractTables[] = { "skills.txt", "levels.txt", "uniqueitems.txt", "setitems.txt", "weapons.txt", "armor.txt", "misc.txt",
	"superuniques.txt", "runes.txt", "monlvl.txt", "monstats.txt", "magicprefix.txt", "magicsuffix.txt", "automagic.txt", "experience.txt",
	"skilldesc.txt", "charstats.txt", "itemtypes.txt", "base/skills.txt", "base/skilldesc.txt", "base/charstats.txt" };

// RotW tables live in excel/, the Classic and LoD tables in excel/base/.
const char* const Banks[] = { "", "base\\" };

// Written next to the mod; the plugin compares it with the connected seed.
constexpr const char* StampFile = "archipelago.txt";

constexpr const char* PluginFile = "d2rl-archipelago.dll";
constexpr const char* ConfigFile = "archipelago.toml";
constexpr const char* LoaderFile = "D2RLoader.exe";
constexpr const wchar_t* LoaderHost = L"d2rloader.net";

struct Options {
	std::string game;
	std::string mod = "Archipelago";
	std::string out;
	std::string extract;
	std::string find;
	std::string dumpPath, dumpFile;
	std::string server, slot, password;
	std::string plugin;
	bool offline = false;
	bool soundsOnly = false;
	bool play = false;
};

auto FindGameFolder() -> std::string {
	char path[MAX_PATH] {};
	DWORD size = sizeof(path);
	if (RegGetValueA(HKEY_LOCAL_MACHINE, "SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Diablo II Resurrected", "InstallLocation",
		    RRF_RT_REG_SZ, nullptr, path, &size) == ERROR_SUCCESS) {
		return path;
	}
	return "C:\\Program Files (x86)\\Diablo II Resurrected";
}

auto ReadCascFile(HANDLE storage, const std::string& name, std::string& contents) -> bool {
	HANDLE file = nullptr;
	if (!CascOpenFile(storage, ("data:" + name).c_str(), 0, CASC_OPEN_BY_NAME, &file)) {
		return false;
	}
	contents.clear();
	char buffer[64 * 1024];
	DWORD read = 0;
	while (CascReadFile(file, buffer, sizeof(buffer), &read) && read > 0) {
		contents.append(buffer, read);
	}
	CascCloseFile(file);
	return !contents.empty();
}

// Decodes a FLAC file with Windows' Media Foundation and writes it as a 16-bit
// PCM WAV, scaled by volume.
auto FlacToWav(const std::string& flac, const fs::path& wav, float volume) -> bool {
	using Microsoft::WRL::ComPtr;
	const fs::path temp = fs::path(wav).replace_extension(".flac");
	std::ofstream(temp, std::ios::binary | std::ios::trunc) << flac;
	std::string pcm;
	std::vector<uint8_t> format;
	{
		ComPtr<IMFSourceReader> reader;
		ComPtr<IMFMediaType>    wanted, actual;
		if (FAILED(MFCreateSourceReaderFromURL(temp.c_str(), nullptr, &reader)) || FAILED(MFCreateMediaType(&wanted))) {
			fs::remove(temp);
			return false;
		}
		wanted->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
		wanted->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
		wanted->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
		WAVEFORMATEX* wave = nullptr;
		UINT32        waveSize = 0;
		if (FAILED(reader->SetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), nullptr, wanted.Get()))
			|| FAILED(reader->GetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), &actual))
			|| FAILED(MFCreateWaveFormatExFromMFMediaType(actual.Get(), &wave, &waveSize))) {
			fs::remove(temp);
			return false;
		}
		format.assign(reinterpret_cast<uint8_t*>(wave), reinterpret_cast<uint8_t*>(wave) + waveSize);
		CoTaskMemFree(wave);
		for (;;) {
			DWORD             flags = 0;
			ComPtr<IMFSample> sample;
			if (FAILED(reader->ReadSample(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), 0, nullptr, &flags, nullptr, &sample))
				|| (flags & MF_SOURCE_READERF_ENDOFSTREAM) != 0) {
				break;
			}
			ComPtr<IMFMediaBuffer> buffer;
			BYTE*                  data = nullptr;
			DWORD                  length = 0;
			if (sample && SUCCEEDED(sample->ConvertToContiguousBuffer(&buffer)) && SUCCEEDED(buffer->Lock(&data, nullptr, &length))) {
				pcm.append(reinterpret_cast<const char*>(data), length);
				buffer->Unlock();
			}
		}
	}
	fs::remove(temp);
	if (pcm.empty()) {
		return false;
	}
	for (size_t i = 0; i + 1 < pcm.size(); i += 2) {
		int16_t sample = 0;
		std::memcpy(&sample, pcm.data() + i, 2);
		sample = static_cast<int16_t>(sample * volume);
		std::memcpy(pcm.data() + i, &sample, 2);
	}
	std::ofstream out(wav, std::ios::binary | std::ios::trunc);
	auto u32 = [&](uint32_t value) { out.write(reinterpret_cast<const char*>(&value), 4); };
	out.write("RIFF", 4);
	u32(static_cast<uint32_t>(4 + 8 + format.size() + 8 + pcm.size()));
	out.write("WAVEfmt ", 8);
	u32(static_cast<uint32_t>(format.size()));
	out.write(reinterpret_cast<const char*>(format.data()), static_cast<std::streamsize>(format.size()));
	out.write("data", 4);
	u32(static_cast<uint32_t>(pcm.size()));
	out << pcm;
	return static_cast<bool>(out);
}

// The banner sounds, converted from the game's own.
void WriteSounds(HANDLE storage, const Options& options) {
	const fs::path folder = fs::path(options.game) / ap::sounds::Folder;
	std::error_code error;
	fs::create_directories(folder, error);
	if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)) || FAILED(MFStartup(MF_VERSION))) {
		std::printf("  couldn't start Media Foundation; the banners stay silent\n");
		return;
	}
	int written = 0;
	for (const auto& sound : ap::sounds::All) {
		std::string flac;
		if (ReadCascFile(storage, sound.source, flac) && FlacToWav(flac, folder / sound.file, ap::sounds::Volume)) {
			++written;
		} else {
			std::printf("  couldn't convert %s for the banners\n", sound.source);
		}
	}
	MFShutdown();
	std::printf("  %d banner sounds written to %s\n", written, folder.string().c_str());
}

auto ReadFile(const fs::path& path) -> std::string {
	std::ifstream file(path, std::ios::binary);
	std::stringstream text;
	text << file.rdbuf();
	return text.str();
}

auto ModRoot(const Options& options) -> fs::path {
	return (options.out.empty() ? fs::path(options.game) / "mods" : fs::path(options.out)) / options.mod;
}

auto ToolDirectory() -> fs::path {
	wchar_t path[MAX_PATH] {};
	GetModuleFileNameW(nullptr, path, MAX_PATH);
	return fs::path(path).parent_path();
}

// The plugin goes in the mod's own d2rloader folder, so it only loads with the
// mod. A global copy from an older setup moves there, with its config (server
// and slot); save state and sounds stay in the game's d2rloader folder.
void InstallPlugin(const Options& options) {
	const fs::path game = options.game;
	const fs::path loader = ModRoot(options) / "d2rloader";
	const fs::path target = loader / "plugins" / PluginFile;
	const fs::path global = game / "d2rloader" / "plugins" / PluginFile;
	const fs::path bundled = options.plugin.empty() ? ToolDirectory() / PluginFile : fs::path(options.plugin);
	std::error_code error;
	fs::create_directories(target.parent_path(), error);
	if (fs::exists(bundled)) {
		if (!fs::exists(target) || ReadFile(bundled) != ReadFile(target)) {
			fs::copy_file(bundled, target, fs::copy_options::overwrite_existing, error);
			if (error) {
				std::printf("Couldn't copy the plugin to %s (%s). Close the game and try again.\n", target.string().c_str(), error.message().c_str());
				return;
			}
			std::printf("Installed the plugin in %s\n", target.parent_path().string().c_str());
		}
	} else if (fs::exists(global)) {
		fs::copy_file(global, target, fs::copy_options::overwrite_existing, error);
		if (error) {
			std::printf("Couldn't move the plugin to %s (%s). Close the game and try again.\n", target.string().c_str(), error.message().c_str());
			return;
		}
	}
	if (!fs::exists(target)) {
		std::printf("No %s next to d2r-ap-setup.exe, so the plugin isn't installed. Put it there, or pass --plugin <file>.\n", PluginFile);
		return;
	}
	const fs::path globalConfig = game / "d2rloader" / "config" / ConfigFile;
	const fs::path modConfig = loader / "config" / ConfigFile;
	if (fs::exists(globalConfig) && !fs::exists(modConfig)) {
		fs::create_directories(modConfig.parent_path(), error);
		fs::copy_file(globalConfig, modConfig, error);
	}
	// A global copy would also load with every other mod.
	if (fs::exists(global)) {
		if (fs::remove(global, error)) {
			std::printf("Moved the plugin from %s into the mod.\n", global.parent_path().string().c_str());
		} else {
			std::printf("Couldn't remove the old plugin %s (%s); delete it by hand.\n", global.string().c_str(), error.message().c_str());
		}
	}
}

// A GET from d2rloader.net over HTTPS.
auto LoaderGet(const wchar_t* path, std::string& body) -> bool {
	HINTERNET session = WinHttpOpen(L"d2r-ap-setup", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	HINTERNET connection = session != nullptr ? WinHttpConnect(session, LoaderHost, INTERNET_DEFAULT_HTTPS_PORT, 0) : nullptr;
	HINTERNET request = connection != nullptr
		? WinHttpOpenRequest(connection, L"GET", path, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE)
		: nullptr;
	bool ok = request != nullptr && WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)
	       && WinHttpReceiveResponse(request, nullptr);
	if (ok) {
		DWORD status = 0;
		DWORD size = sizeof(status);
		ok = WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &size,
		         WINHTTP_NO_HEADER_INDEX)
		  && status == 200;
	}
	while (ok) {
		DWORD available = 0;
		if (!WinHttpQueryDataAvailable(request, &available)) {
			ok = false;
		} else if (available == 0) {
			break;
		} else {
			const size_t offset = body.size();
			body.resize(offset + available);
			DWORD read = 0;
			ok = WinHttpReadData(request, body.data() + offset, available, &read);
			body.resize(offset + read);
		}
	}
	for (HINTERNET handle : { request, connection, session }) {
		if (handle != nullptr) {
			WinHttpCloseHandle(handle);
		}
	}
	return ok;
}

auto LatestLoaderVersion() -> std::string {
	std::string body;
	if (!LoaderGet(L"/api/v1/version", body)) {
		return {};
	}
	const auto json = nlohmann::json::parse(body, nullptr, false);
	return json.is_object() && json.contains("version") && json["version"].is_string() ? json["version"].get<std::string>() : "";
}

// An exe's ProductVersion, e.g. "1.3.1-beta".
auto ProductVersion(const fs::path& exe) -> std::string {
	DWORD handle = 0;
	const DWORD size = GetFileVersionInfoSizeW(exe.c_str(), &handle);
	std::vector<char> data(size);
	if (size == 0 || !GetFileVersionInfoW(exe.c_str(), 0, size, data.data())) {
		return {};
	}
	struct Translation {
		WORD language, codePage;
	}* translation = nullptr;
	UINT length = 0;
	if (!VerQueryValueW(data.data(), L"\\VarFileInfo\\Translation", reinterpret_cast<void**>(&translation), &length) || length < sizeof(Translation)) {
		return {};
	}
	wchar_t key[64] {};
	swprintf_s(key, L"\\StringFileInfo\\%04x%04x\\ProductVersion", translation->language, translation->codePage);
	wchar_t* value = nullptr;
	if (!VerQueryValueW(data.data(), key, reinterpret_cast<void**>(&value), &length) || length == 0) {
		return {};
	}
	return fs::path(value).string();
}

// D2RLoader comes from its own site, not from this tool: Defender flags it as a
// trojan, and a program that downloads and unpacks it gets flagged as well.
constexpr const char* LoaderPage = "https://d2rloader.net/download.html";

// When D2RLoader is missing, opens its download page and says what to do. A
// newer one is only mentioned: D2RLoader updates can break the plugin.
auto CheckLoader(const Options& options) -> bool {
	const fs::path game = options.game;
	if (!fs::exists(game / LoaderFile)) {
		ShellExecuteA(nullptr, "open", LoaderPage, nullptr, nullptr, SW_SHOWNORMAL);
		std::printf("\nD2RLoader isn't installed. Its download page is opening in your browser (%s):\n"
		            "  1. Download D2RLoader and extract the zip into %s\n"
		            "  2. If Windows Defender blocks or removes it (it often flags D2RLoader, which patches the game\n"
		            "     while it runs), allow it under Windows Security, Virus & threat protection, Protection history.\n"
		            "  3. Then do this again.\n",
			LoaderPage, game.string().c_str());
		return false;
	}
	const std::string installed = ProductVersion(game / LoaderFile);
	const std::string latest = LatestLoaderVersion();
	if (!latest.empty() && latest != installed) {
		std::printf("D2RLoader %s is out (you have %s). Get it from %s when you want it.\n", latest.c_str(), installed.c_str(), LoaderPage);
	}
	return true;
}

auto ParseArgs(int argc, char** argv) -> Options {
	Options options;
	options.game = FindGameFolder();
	for (int i = 1; i < argc; ++i) {
		const std::string arg = argv[i];
		auto next = [&]() -> std::string { return i + 1 < argc ? argv[++i] : ""; };
		if (arg == "--game") {
			options.game = next();
		} else if (arg == "--mod") {
			options.mod = next();
		} else if (arg == "--out") {
			options.out = next();
		} else if (arg == "--find") {
			options.find = next();
		} else if (arg == "--extract") {
			options.extract = next();
		} else if (arg == "--dump") {
			options.dumpPath = next();
			options.dumpFile = next();
		} else if (arg == "--connect") {
			options.server = next();
			options.slot = next();
		} else if (arg == "--password") {
			options.password = next();
		} else if (arg == "--offline") {
			options.offline = true;
		} else if (arg == "--sounds") {
			options.soundsOnly = true;
		} else if (arg == "--plugin") {
			options.plugin = next();
		} else if (arg == "--play") {
			options.play = true;
		}
	}
	return options;
}

int Extract(HANDLE storage, const Options& options) {
	fs::create_directories(options.extract);
	int copied = 0;
	for (const char* table : ExtractTables) {
		std::string contents;
		std::string name = table;
		std::ranges::replace(name, '/', '\\');
		if (ReadCascFile(storage, "data\\global\\excel\\" + name, contents)) {
			const fs::path target = fs::path(options.extract) / table;
			fs::create_directories(target.parent_path());
			std::ofstream(target, std::ios::binary | std::ios::trunc) << contents;
			++copied;
		} else {
			std::printf("  %s: not found\n", table);
		}
	}
	std::printf("Extracted %d tables to %s\n", copied, options.extract.c_str());
	return copied == static_cast<int>(std::size(ExtractTables)) ? 0 : 1;
}

struct Seed {
	std::string name, slot;
	rnd::Settings settings;
	std::vector<std::pair<std::string, std::string>> loaderSettings;
};

// Connects to the server just long enough to read the slot's settings.
auto FetchSeed(const Options& options, Seed& seed) -> bool {
	std::printf("Connecting to %s as %s...\n", options.server.c_str(), options.slot.c_str());
	ap::Client client([](const std::string&) {});
	client.Connect(options.server, options.slot, options.password);
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
	while (client.State() != ap::ConnectionState::Connected && client.State() != ap::ConnectionState::Refused
		   && std::chrono::steady_clock::now() < deadline) {
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	if (client.State() != ap::ConnectionState::Connected) {
		std::printf("%s\n", client.StatusText().c_str());
		return false;
	}
	seed = { client.Seed(), client.Slot(), client.Slots().randomizer, client.Slots().loaderSettings };
	client.Disconnect();
	return true;
}

// Reads one table from a bank, edits it, and writes it to the mod if the edit
// changed anything.
auto PatchTable(HANDLE storage, const fs::path& excel, const std::string& bank, const std::string& name, const std::function<void(Table&)>& edit) -> bool {
	std::string contents;
	if (!ReadCascFile(storage, "data\\global\\excel\\" + bank + name, contents)) {
		return false;
	}
	Table table = Table::Parse(contents);
	edit(table);
	const std::string patched = table.Format();
	if (patched == contents) {
		return false;
	}
	const fs::path target = excel / (bank.empty() ? fs::path() : fs::path("base")) / name;
	fs::create_directories(target.parent_path());
	std::ofstream(target, std::ios::binary | std::ios::trunc) << patched;
	return true;
}

// Names the items this tool adds: D2R shows "An Evil Force" for a unique whose
// name has no string. Appends an entry per name, the same in every language,
// to the game's own item-names.json.
auto AddItemNames(HANDLE storage, const fs::path& mpq, const std::vector<std::string>& names) -> bool {
	constexpr const char* Path = "data\\local\\lng\\strings\\item-names.json";
	std::string text;
	if (!ReadCascFile(storage, Path, text)) {
		return false;
	}
	const bool bom = text.rfind("\xEF\xBB\xBF", 0) == 0;
	auto strings = nlohmann::json::parse(bom ? text.substr(3) : text, nullptr, false);
	if (!strings.is_array() || strings.empty()) {
		return false;
	}
	int maxId = 0;
	std::set<std::string> keys;
	for (const auto& entry : strings) {
		maxId = std::max(maxId, entry.value("id", 0));
		keys.insert(entry.value("Key", ""));
	}
	for (const std::string& name : names) {
		if (keys.contains(name)) {
			continue;
		}
		nlohmann::json added = { { "id", ++maxId }, { "Key", name } };
		for (const auto& [key, value] : strings.front().items()) {
			if (key != "id" && key != "Key" && value.is_string()) {
				added[key] = name;
			}
		}
		strings.push_back(added);
	}
	const fs::path target = mpq / "data" / "local" / "lng" / "strings" / "item-names.json";
	fs::create_directories(target.parent_path());
	std::ofstream(target, std::ios::binary | std::ios::trunc) << (bom ? "\xEF\xBB\xBF" : "") << strings.dump(2);
	return true;
}

// Each class's skill icon sheet, under data/hd/global/ui/spells/.
const std::map<std::string, std::string> IconSheets = {
	{ "ama", "amazon/amskillicon" }, { "sor", "sorceress/soskillicon" }, { "nec", "necromancer/neskillicon" },
	{ "pal", "paladin/paskillicon" }, { "bar", "barbarian/baskillicon" }, { "dru", "druid/drskillicon" },
	{ "ass", "assassin/asskillicon" },
};

// Each class's icon sheet length, for the classes whose HD and low-end sheets
// agree (tree mode's locked icons go after them).
auto IconFrames(HANDLE storage) -> std::map<std::string, int> {
	std::map<std::string, int> frames;
	for (const auto& [cls, sheet] : IconSheets) {
		std::optional<uint32_t> count;
		for (const char* suffix : { ".sprite", ".lowend.sprite" }) {
			std::string bytes;
			std::string path = "data/hd/global/ui/spells/" + sheet + suffix;
			std::ranges::replace(path, '/', '\\');
			const auto parsed = ReadCascFile(storage, path, bytes) ? ap::sprite::Parse(std::move(bytes)) : std::nullopt;
			if (!parsed || (count && *count != parsed->frames)) {
				count.reset();
				break;
			}
			count = parsed->frames;
		}
		if (count) {
			frames[cls] = static_cast<int>(*count);
		}
	}
	return frames;
}

// Tree mode's list of locked icons for one bank, for the plugin.
auto LockedIconsFile(size_t bankIndex) -> std::string {
	return bankIndex == 0 ? "locked-icons.txt" : "locked-icons-lod.txt";
}

// After a skill tree shuffle, rebuilds every class's skill icon sheets (HD and
// low-end) so each skill shows its own icon in its new tree: the skill's frames
// (icon and pressed icon) are copied from its old class's sheet to its new slot.
// With `locked` (tree mode), each sheet of `lockedClasses` also gets its locked copies.
auto WriteSkillIcons(HANDLE storage, const fs::path& mpq, const std::vector<rnd::IconMove>& icons, const std::map<std::string, int>& lockedClasses)
    -> int {
	int written = 0;
	for (const char* suffix : { ".sprite", ".lowend.sprite" }) {
		std::map<std::string, ap::sprite::Sheet> original, rebuilt;
		for (const auto& [cls, sheet] : IconSheets) {
			std::string bytes;
			std::string path = "data/hd/global/ui/spells/" + sheet + suffix;
			std::ranges::replace(path, '/', '\\');
			if (ReadCascFile(storage, path, bytes)) {
				if (auto parsed = ap::sprite::Parse(std::move(bytes))) {
					original.emplace(cls, *parsed);
					rebuilt.emplace(cls, *parsed);
				}
			}
		}
		std::set<std::string> changed;
		for (const auto& icon : icons) {
			const auto from = original.find(icon.fromClass);
			const auto to = rebuilt.find(icon.toClass);
			if (from == original.end() || to == rebuilt.end()) {
				continue;
			}
			for (int frame = 0; frame < 2; ++frame) {
				if (ap::sprite::CopyFrame(from->second, static_cast<uint32_t>(icon.fromCel + frame), to->second, static_cast<uint32_t>(icon.toCel + frame))) {
					changed.insert(icon.toClass);
				}
			}
		}
		for (auto& [cls, sheet] : rebuilt) {
			if (const auto frames = lockedClasses.find(cls); frames != lockedClasses.end() && sheet.frames == static_cast<uint32_t>(frames->second)) {
				ap::sprite::AddLockedFrames(sheet);
				changed.insert(cls);
			}
		}
		for (const auto& cls : changed) {
			const fs::path target = mpq / "data" / "hd" / "global" / "ui" / "spells" / fs::path(IconSheets.at(cls) + suffix);
			fs::create_directories(target.parent_path());
			std::ofstream(target, std::ios::binary | std::ios::trunc) << rebuilt.at(cls).bytes;
			++written;
		}
	}
	return written;
}

// The skill tree panels draw each class's prerequisite arrows from one sprite
// per class: a frame per tree with the tab header, the skill boxes and the
// arrows. After a shuffle the arrows point the wrong way (and the tree's own
// prerequisites are gone), so they're cleared: the mouse panel and the
// controller panel, HD and low-end. Sizes are fractions of a frame's width,
// between an arrow's width and a level box's.
struct TreeArt {
	const char* folder;
	double      kernel;
	double      minSize;
};
constexpr TreeArt TreeArts[] = { { "data/hd/global/ui/spells/skill_trees/", 0.063, 0.06 },
	{ "data/hd/global/ui/controller/panel/spells/v2/", 0.033, 0.035 } };

auto WriteSkillTreeArt(HANDLE storage, const fs::path& mpq) -> int {
	int written = 0;
	for (const auto& art : TreeArts) {
		for (const auto& [cls, sheet] : IconSheets) {
			// "barbarian/baskillicon" -> "baskilltree"
			const std::string name = sheet.substr(sheet.find('/') + 1, 2) + "skilltree";
			for (const char* suffix : { ".sprite", ".lowend.sprite" }) {
				std::string path = art.folder + name + suffix;
				std::string casc = path;
				std::ranges::replace(casc, '/', '\\');
				std::string bytes;
				if (!ReadCascFile(storage, casc, bytes)) {
					continue;
				}
				auto parsed = ap::sprite::Parse(std::move(bytes));
				if (!parsed) {
					continue;
				}
				const double frame = parsed->FrameWidth();
				ap::sprite::RemoveThinShapes(*parsed, static_cast<uint32_t>(frame * art.kernel) | 1U, static_cast<uint32_t>(frame * art.minSize));
				const fs::path target = mpq / fs::path(path);
				fs::create_directories(target.parent_path());
				std::ofstream(target, std::ios::binary | std::ios::trunc) << parsed->bytes;
				++written;
			}
		}
	}
	return written;
}

// Gives the shop parcel an HD picture: items.json maps each item code to a
// sprite, and a code without one shows nothing. The parcel borrows the
// Standard of Heroes' flag, like its misc.txt row.
auto AddParcelGraphics(HANDLE storage, const fs::path& mpq) -> bool {
	constexpr const char* Path = "data\\hd\\items\\items.json";
	std::string text;
	if (!ReadCascFile(storage, Path, text)) {
		return false;
	}
	const bool bom = text.rfind("\xEF\xBB\xBF", 0) == 0;
	auto items = nlohmann::json::parse(bom ? text.substr(3) : text, nullptr, false);
	if (!items.is_array()) {
		return false;
	}
	// Parcels borrow the Standard of Heroes' flag, Scrolls of Prophecy the Scroll of Inifuss,
	// the Emblem and the Writ of Absolution the Token of Absolution.
	nlohmann::json graphics, scroll, token;
	for (const auto& entry : items) {
		if (entry.contains(rnd::ParcelCodes[0])) {
			return true;
		}
		if (entry.contains("std")) {
			graphics = entry["std"];
		}
		if (entry.contains("bks")) {
			scroll = entry["bks"];
		}
		if (entry.contains("toa")) {
			token = entry["toa"];
		}
	}
	if (graphics.is_null()) {
		return false;
	}
	for (const char* code : rnd::ParcelCodes) {
		items.push_back({ { code, graphics } });
	}
	for (const char* code : rnd::ProphecyCodes) {
		items.push_back({ { code, scroll.is_null() ? graphics : scroll } });
	}
	items.push_back({ { rnd::EmblemCode, token.is_null() ? graphics : token } });
	items.push_back({ { rnd::RespecCode, token.is_null() ? graphics : token } });
	const fs::path target = mpq / "data" / "hd" / "items" / "items.json";
	fs::create_directories(target.parent_path());
	std::ofstream(target, std::ios::binary | std::ios::trunc) << (bom ? "\xEF\xBB\xBF" : "") << items.dump(2);
	return true;
}

// Gives each superunique's own monster (rnd::AddSuperUniqueMonsters) the HD
// model of the monster it copies: monsters.json maps each monstats Id to a
// model, and an Id without one is invisible.
auto AddSuperUniqueModels(HANDLE storage, const fs::path& mpq, const std::vector<std::pair<std::string, std::string>>& copies) -> int {
	constexpr const char* Path = "data\\hd\\character\\monsters.json";
	std::string text;
	if (copies.empty() || !ReadCascFile(storage, Path, text)) {
		return 0;
	}
	const bool bom = text.rfind("\xEF\xBB\xBF", 0) == 0;
	auto models = nlohmann::ordered_json::parse(bom ? text.substr(3) : text, nullptr, false);
	if (!models.is_object()) {
		return 0;
	}
	// Both banks share the file; the later bank's copy (Classic/LoD's) wins.
	std::set<std::string> added;
	for (const auto& [copy, original] : copies) {
		if (models.contains(original)) {
			// Copy first: adding a key to an ordered_json moves its entries.
			const auto model = models[original];
			models[copy] = model;
			added.insert(copy);
		}
	}
	const fs::path target = mpq / "data" / "hd" / "character" / "monsters.json";
	fs::create_directories(target.parent_path());
	std::ofstream(target, std::ios::binary | std::ios::trunc) << (bom ? "\xEF\xBB\xBF" : "") << models.dump(2);
	return static_cast<int>(added.size());
}

// A generator per table, so each table's shuffle depends only on the seed and
// the table (FNV-1a of its name), never on which other tables were edited.
auto TableRandom(uint32_t seed, const std::string& table) -> std::mt19937 {
	uint32_t hash = 2166136261U;
	for (const char c : table) {
		hash = (hash ^ static_cast<uint8_t>(c)) * 16777619U;
	}
	return std::mt19937(seed ^ hash);
}

int WriteMod(HANDLE storage, const Options& options, const Seed* seed) {
	const fs::path modRoot = ModRoot(options);
	const fs::path mpq = modRoot / (options.mod + ".mpq");
	const fs::path excel = mpq / "data" / "global" / "excel";
	// Clear the previous seed's tables, but only in a mod this tool made.
	if (fs::exists(excel) && !fs::exists(modRoot / StampFile)) {
		std::printf("%s exists but wasn't made by this tool; pick another name with --mod.\n", modRoot.string().c_str());
		return 1;
	}
	std::error_code error;
	fs::remove_all(excel, error);
	// And the previous seed's skill icon sheets, which only a tree shuffle or tree mode writes.
	fs::remove_all(mpq / "data" / "hd" / "global" / "ui" / "spells", error);
	for (size_t bankIndex = 0; bankIndex < std::size(Banks); ++bankIndex) {
		fs::remove(modRoot / LockedIconsFile(bankIndex), error);
	}
	fs::remove_all(mpq / "data" / "hd" / "global" / "ui" / "controller" / "panel" / "spells", error);
	// And the superunique models, which only Superunique Checks write.
	fs::remove(mpq / "data" / "hd" / "character" / "monsters.json", error);
	if (!AddItemNames(storage, mpq, { "Archipelago Codex", rnd::ParcelName, rnd::ProphecyName, rnd::EmblemName, rnd::RespecName })) {
		std::printf("  couldn't add the Codex's name; it will show as \"An Evil Force\"\n");
	}
	if (!AddParcelGraphics(storage, mpq)) {
		std::printf("  couldn't give the shop parcel a picture\n");
	}

	const rnd::Settings settings = seed != nullptr ? seed->settings : rnd::Settings {};
	std::vector<std::string> recipes;
	// Both banks shuffle the trees alike, so either one's icon moves will do.
	std::vector<rnd::IconMove> icons;
	// Tree mode: each class's icon sheet length, where its locked icons start.
	const auto iconFrames = settings.treeMode ? IconFrames(storage) : std::map<std::string, int> {};
	// RotW's misc codes in order, so Classic/LoD can hold the places of the ones it lacks.
	std::vector<std::string> rotwMisc;
	// Each superunique's own monster and the one it copies, from either bank (both share monsters.json).
	std::vector<std::pair<std::string, std::string>> superUniqueCopies;
	{
		std::string text;
		ReadCascFile(storage, "data\\global\\excel\\misc.txt", text);
		const Table misc = Table::Parse(text);
		if (const int code = misc.Column("code"); code >= 0) {
			for (size_t row = 0; row < misc.Rows().size(); ++row) {
				rotwMisc.push_back(misc.Cell(row, code));
			}
		}
	}
	// Both banks' runes.txt, shuffled together: a runeword both banks have gets one recipe, so
	// runewords.txt is right for any character. A word only one bank makes this way says so.
	std::vector<std::optional<Table>> runeTables(std::size(Banks));
	if (settings.runewords) {
		std::vector<Table*> tables;
		std::vector<const char*> labels;
		for (size_t i = 0; i < std::size(Banks); ++i) {
			std::string text;
			if (ReadCascFile(storage, std::string("data\\global\\excel\\") + Banks[i] + "runes.txt", text)) {
				runeTables[i] = Table::Parse(text);
				tables.push_back(&*runeTables[i]);
				labels.push_back(std::string(Banks[i]).empty() ? "Reign of the Warlock" : "Classic/LoD");
			}
		}
		auto random = TableRandom(settings.seed, "runes");
		for (auto& recipe : rnd::ShuffleRunewords(tables, random, settings.runeSpread)) {
			if (recipe.banks.size() < tables.size()) {
				std::string only;
				for (const size_t index : recipe.banks) {
					only += (only.empty() ? "" : ", ") + std::string(labels[index]);
				}
				recipe.text += " (" + only + " only)";
			}
			recipes.push_back(std::move(recipe.text));
		}
	}
	for (size_t bankIndex = 0; bankIndex < std::size(Banks); ++bankIndex) {
		const std::string bank = Banks[bankIndex];
		const char* label = bank.empty() ? "RotW" : "Classic/LoD";
		int blocked = 0;
		if (!settings.skillTree.empty() || !iconFrames.empty()) {
			// The trees span two tables: skills.txt holds each skill's class, skilldesc.txt its place and icon.
			std::string descText;
			ReadCascFile(storage, "data\\global\\excel\\" + bank + "skilldesc.txt", descText);
			Table skilldesc = Table::Parse(descText);
			int moved = 0;
			std::vector<rnd::LockedIcon> locked;
			PatchTable(storage, excel, bank, "skills.txt", [&](Table& t) {
				if (!settings.skillTree.empty()) {
					auto shuffle = rnd::ApplySkillTrees(t, skilldesc, settings.skillTree);
					moved = shuffle.moved;
					icons = std::move(shuffle.icons);
				}
				blocked = rnd::BlockClassSkills(t, settings.treeMode);
				locked = rnd::LockSkillIcons(t, skilldesc, iconFrames);
			});
			PatchTable(storage, excel, bank, "skilldesc.txt", [&](Table& t) { t = skilldesc; });
			if (!settings.skillTree.empty()) {
				std::printf("  %s: %d skills moved to another class's tree\n", label, moved);
			}
			if (!locked.empty()) {
				std::ofstream(modRoot / LockedIconsFile(bankIndex), std::ios::binary | std::ios::trunc) << rnd::FormatLockedIcons(locked);
				std::printf("  %s: %zu locked skills show a padlock until they unlock\n", label, locked.size());
			}
		} else {
			PatchTable(storage, excel, bank, "skills.txt", [&](Table& t) { blocked = rnd::BlockClassSkills(t, settings.treeMode); });
		}
		std::printf("  %s: locked %d class skills\n", label, blocked);
		if (!settings.startClass.empty()) {
			// Skill names don't change in the shuffle, so the game's own skills.txt names it.
			std::string skillsText;
			ReadCascFile(storage, "data\\global\\excel\\" + bank + "skills.txt", skillsText);
			const Table skills = Table::Parse(skillsText);
			bool found = false;
			PatchTable(storage, excel, bank, "charstats.txt",
			           [&](Table& t) { found = rnd::SetStartSkill(t, skills, settings.startClass, settings.startSkill); });
			if (found) {
				std::printf("  %s: %s starting weapon skill set\n", label, settings.startClass.c_str());
			}
		}

		const rnd::PerDifficulty unchanged = { 100, 100, 100 };
		const auto& density = settings.monsterDensity;
		if (settings.monsters != rnd::Shuffle::Off || density != unchanged || settings.championPacks != 100) {
			std::string stats;
			ReadCascFile(storage, "data\\global\\excel\\" + bank + "monstats.txt", stats);
			const Table monstats = Table::Parse(stats);
			int changed = 0;
			auto random = TableRandom(settings.seed, "levels");
			PatchTable(storage, excel, bank, "levels.txt", [&](Table& t) {
				changed = rnd::ShuffleMonsters(t, monstats, settings.monsters, random, settings.shuffleTolerance, settings.shuffleCowLevel);
				rnd::ScaleDensity(t, density, settings.championPacks);
			});
			std::printf("  %s: new monsters in %d areas, density %d/%d/%d%%, champion packs %d%%\n", label, changed, density[0], density[1], density[2],
			            settings.championPacks);
		}
		if (settings.betterDrops != 100 || settings.betterMagic != 100) {
			PatchTable(storage, excel, bank, "itemratio.txt", [&](Table& t) { rnd::BoostDrops(t, settings.betterDrops, settings.betterMagic); });
			std::printf("  %s: uniques, sets and rares drop at %d%%, magic items at %d%%\n", label, settings.betterDrops, settings.betterMagic);
		}
		const bool ownMonsters = settings.SuperUniqueCopies();
		if (settings.superUniques != rnd::Shuffle::Off || ownMonsters || settings.superUniqueMinions != 100) {
			std::string stats;
			ReadCascFile(storage, "data\\global\\excel\\" + bank + "monstats.txt", stats);
			Table monstats = Table::Parse(stats);
			int changed = 0, linked = 0, minions = 0, tuned = 0;
			auto random = TableRandom(settings.seed, "superuniques");
			PatchTable(storage, excel, bank, "superuniques.txt", [&](Table& t) {
				const Table before = t;
				changed = rnd::ShuffleSuperUniques(t, monstats, settings.superUniques, random, settings.mainPathSuperUniqueCap, settings.superUniqueSpread);
				minions = rnd::ScaleSuperUniqueMinions(t, before, settings.superUniqueMinions, settings.mainPathSuperUniqueMinions);
				if (ownMonsters) {
					linked = rnd::AddSuperUniqueMonsters(monstats, t, &superUniqueCopies);
					tuned = rnd::TuneMainPathSuperUniques(monstats, before, settings.mainPathSuperUniqueStrength, settings.mainPathSuperUniqueLevel);
				}
			});
			if (ownMonsters) {
				PatchTable(storage, excel, bank, "monstats.txt", [&](Table& t) { t = monstats; });
			}
			std::printf("  %s: %d superuniques changed shape, %d got a new minion count, %d a new level or strength, %d have their own monster%s\n", label,
			            changed, minions, tuned, linked, settings.superUniqueChecks ? " (checks)" : "");
		}
		if (runeTables[bankIndex]) {
			PatchTable(storage, excel, bank, "runes.txt", [&](Table& t) { t = *runeTables[bankIndex]; });
		}
		if (const auto& xp = settings.xpPercent; xp != unchanged) {
			PatchTable(storage, excel, bank, "monlvl.txt", [&](Table& t) { rnd::ScaleExperience(t, xp); });
			std::printf("  %s: monster experience at %d/%d/%d%%\n", label, xp[0], xp[1], xp[2]);
		}
		if (settings.deathXpPenalty != 100) {
			bool found = false;
			PatchTable(storage, excel, bank, "difficultylevels.txt", [&](Table& t) { found = rnd::ScaleDeathPenalty(t, settings.deathXpPenalty); });
			if (found) {
				std::printf("  %s: experience lost on death at %d%%\n", label, settings.deathXpPenalty);
			} else {
				std::printf("  %s: couldn't change the experience lost on death (difficultylevels.txt has no DeathExpPenalty)\n", label);
			}
		}
		const bool clearRequirements = !settings.itemLevelRequirements || !settings.itemStatRequirements;
		if (clearRequirements) {
			for (const char* table : { "weapons.txt", "armor.txt", "setitems.txt" }) {
				PatchTable(storage, excel, bank, table, [&](Table& t) { rnd::ClearRequirements(t, !settings.itemLevelRequirements, !settings.itemStatRequirements); });
			}
			std::printf("  %s: item requirements cleared\n", label);
		}
		// Misc gets the shop parcel, plus cleared requirements if asked.
		int parcels = 0;
		PatchTable(storage, excel, bank, "itemtypes.txt", [&](Table& t) { rnd::AddShopItemType(t); });
		PatchTable(storage, excel, bank, "misc.txt", [&](Table& t) {
			if (clearRequirements) {
				rnd::ClearRequirements(t, !settings.itemLevelRequirements, !settings.itemStatRequirements);
			}
			if (!bank.empty()) {
				rnd::PadMiscRows(t, rotwMisc);
			}
			parcels = rnd::AddShopParcels(t) + rnd::AddProphecyScrolls(t);
			rnd::AddEmblem(t);
			rnd::AddRespecWrit(t);
		});
		std::printf("  %s: %s\n", label, parcels > 0 ? "each act's potion seller sells an Archipelago Parcel" : "couldn't add the shop parcels");
		// UniqueItems gets the Codex charm, plus cleared requirements if asked.
		PatchTable(storage, excel, bank, "uniqueitems.txt", [&](Table& t) {
			if (clearRequirements) {
				rnd::ClearRequirements(t, !settings.itemLevelRequirements, !settings.itemStatRequirements);
			}
			rnd::AddCodexUnique(t, ap::data::CodexUnique);
		});
	}

	if (!superUniqueCopies.empty()) {
		std::printf("  %d superunique monsters given their HD models\n", AddSuperUniqueModels(storage, mpq, superUniqueCopies));
	}
	if (!icons.empty()) {
		const int trees = WriteSkillTreeArt(storage, mpq);
		std::printf("  %d skill tree panels redrawn without arrows", trees);
		std::puts("");
	}
	if (!icons.empty() || !iconFrames.empty()) {
		std::printf("  %d skill icon sheets rebuilt\n", WriteSkillIcons(storage, mpq, icons, iconFrames));
	}
	if (!recipes.empty()) {
		std::ofstream list(modRoot / "runewords.txt", std::ios::trunc);
		list << "Runeword recipes for this seed:\n\n";
		for (const auto& recipe : recipes) {
			list << recipe << "\n";
		}
		std::printf("  %zu runeword recipes changed; see %s\n", recipes.size(), (modRoot / "runewords.txt").string().c_str());
	} else {
		fs::remove(modRoot / "runewords.txt", error);
	}
	const fs::path modinfo = mpq / "modinfo.json";
	if (!fs::exists(modinfo)) {
		fs::create_directories(mpq);
		std::ofstream(modinfo) << "{\n\t\"name\": \"" << options.mod << "\",\n\t\"savepath\": \"" << options.mod << "/\"\n}\n";
	}
	std::ofstream(modRoot / StampFile, std::ios::trunc) << (seed != nullptr ? seed->name : "") << "\n"
	                                                  << (seed != nullptr ? seed->slot : "") << "\n"
	                                                  << settings.Stamp() << "\n";
	std::printf("Wrote the %s mod to %s\n", options.mod.c_str(), modRoot.string().c_str());
	return 0;
}

// Saves the server and slot in the plugin's config in the mod, so it connects
// as soon as the game starts. The player's other settings are kept.
void SaveConnection(const Options& options) {
	const fs::path path = ModRoot(options) / "d2rloader" / "config" / ConfigFile;
	ap::Config config = ap::ParseConfig(ReadFile(path));
	config.server = options.server;
	config.slot = options.slot;
	config.password = options.password;
	config.autoConnect = true;
	std::error_code error;
	fs::create_directories(path.parent_path(), error);
	std::ofstream(path, std::ios::binary | std::ios::trunc) << ap::FormatConfig(config);
}

// Writes the seed's D2RLoader settings (stash tabs, item labels...) as the
// mod's overrides, replacing the last seed's. Without any, the file only has
// its header, so the player's own D2RLoader settings apply.
void WriteLoaderSettings(const Options& options, const Seed& seed) {
	const fs::path path = ModRoot(options) / "d2rloader" / "config" / "d2rloader.toml";
	std::error_code error;
	fs::create_directories(path.parent_path(), error);
	std::ofstream(path, std::ios::binary | std::ios::trunc) << ap::FormatLoaderSettings(seed.loaderSettings);
	for (const auto& [setting, value] : seed.loaderSettings) {
		std::printf("  D2RLoader setting %s = %s\n", setting.c_str(), value.c_str());
	}
}

// Starts D2RLoader with the mod, the way its own menu would.
auto Play(const Options& options) -> bool {
	const fs::path game = options.game;
	std::wstring command = L"\"" + (game / LoaderFile).wstring() + L"\" -mod " + fs::path(options.mod).wstring() + L" -txt";
	STARTUPINFOW startup { .cb = sizeof(startup) };
	PROCESS_INFORMATION process {};
	if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, 0, nullptr, game.c_str(), &startup, &process)) {
		std::printf("Couldn't start %s (error %lu).\n", (game / LoaderFile).string().c_str(), GetLastError());
		return false;
	}
	CloseHandle(process.hThread);
	CloseHandle(process.hProcess);
	std::printf("Starting Diablo II: Resurrected with the %s mod.\n", options.mod.c_str());
	return true;
}

int Run(Options options) {
	std::printf("Diablo II: Resurrected folder: %s\n", options.game.c_str());

	HANDLE storage = nullptr;
	if (!CascOpenStorage(options.game.c_str(), 0, &storage)) {
		std::printf("Couldn't open the game files (error %lu). Pass the D2R folder with --game.\n", GetLastError());
		return 1;
	}
	if (!options.find.empty()) {
		CASC_FIND_DATA data {};
		HANDLE search = CascFindFirstFile(storage, "*", &data, nullptr);
		int found = 0;
		for (bool more = search != nullptr; more; more = CascFindNextFile(search, &data)) {
			if (std::string_view(data.szFileName).find(options.find) != std::string_view::npos) {
				std::printf("%s (%llu bytes)\n", data.szFileName, static_cast<unsigned long long>(data.FileSize));
				++found;
			}
		}
		if (search != nullptr) {
			CascFindClose(search);
		}
		CascCloseStorage(storage);
		std::printf("%d files\n", found);
		return 0;
	}
	if (options.soundsOnly) {
		WriteSounds(storage, options);
		CascCloseStorage(storage);
		return 0;
	}
	if (!options.extract.empty()) {
		const int result = Extract(storage, options);
		CascCloseStorage(storage);
		return result;
	}
	if (!options.dumpPath.empty()) {
		std::string contents;
		const bool found = ReadCascFile(storage, options.dumpPath, contents);
		CascCloseStorage(storage);
		if (!found) {
			std::printf("%s: not found\n", options.dumpPath.c_str());
			return 1;
		}
		std::ofstream(options.dumpFile, std::ios::binary | std::ios::trunc) << contents;
		std::printf("Wrote %zu bytes to %s\n", contents.size(), options.dumpFile.c_str());
		return 0;
	}

	// A mod written elsewhere (--out) is only for testing; leave the game alone.
	if (options.out.empty()) {
		if (!CheckLoader(options)) {
			CascCloseStorage(storage);
			return 1;
		}
		InstallPlugin(options);
	}

	// Without --connect, use what the plugin saved after ap-connect: in the mod,
	// or in the game's d2rloader folder before the plugin moved into the mod.
	if (!options.offline && options.server.empty()) {
		const fs::path modConfig = ModRoot(options) / "d2rloader" / "config" / ConfigFile;
		const auto config = ap::ParseConfig(ReadFile(fs::exists(modConfig) ? modConfig : fs::path(options.game) / "d2rloader" / "config" / ConfigFile));
		options.server = config.server;
		options.slot = config.slot;
		options.password = config.password;
	}
	Seed seed;
	bool haveSeed = false;
	if (!options.offline && !options.server.empty()) {
		haveSeed = FetchSeed(options, seed);
		if (!haveSeed) {
			CascCloseStorage(storage);
			std::printf("Couldn't read the seed from %s as %s. Check that the server is running (an archipelago.gg room\n"
			            "starts again when you open its page) and that the slot name is right.\n",
				options.server.c_str(), options.slot.c_str());
			return 1;
		}
		std::printf("Seed %s, slot %s: %s\n", seed.name.c_str(), seed.slot.c_str(), seed.settings.Stamp().c_str());
		if (options.out.empty()) {
			SaveConnection(options);
		}
	} else if (options.play) {
		CascCloseStorage(storage);
		std::printf("No server to connect to. Click your name on the room page, or use --connect <server> <slot>.\n");
		return 1;
	} else {
		std::printf("No server given, so only the skill lock is written. Use --connect <server> <slot> for a seed's randomization.\n");
	}
	const int result = WriteMod(storage, options, haveSeed ? &seed : nullptr);
	if (result == 0 && haveSeed) {
		WriteLoaderSettings(options, seed);
	}
	WriteSounds(storage, options);
	CascCloseStorage(storage);
	return result;
}

} // namespace

int main(int argc, char** argv) {
	const Options options = ParseArgs(argc, argv);
	const int result = Run(options);
	if (!options.play) {
		return result;
	}
	if (result == 0 && Play(options)) {
		return 0;
	}
	// Opened from the Archipelago Launcher, the window would close before the
	// player could read what went wrong.
	std::printf("\nPress Enter to close.\n");
	std::getchar();
	return 1;
}
