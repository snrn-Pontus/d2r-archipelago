// Archipelago multiworld client for Diablo II: Resurrected on D2RLoader.
//
// Threads: gameplay events and console commands arrive on the UI thread,
// overlay frames on the render thread, item changes run on the game thread,
// and ap::Client owns a network thread. Everything shared lives under gMutex.

#include "ap_client.h"
#include "codex.h"
#include "config.h"
#include "game_data.h"
#include "ids.h"
#include "loot.h"
#include "quest_triggers.h"
#include "save_state.h"

#include <D2RLPlugin/api.h>

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <deque>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <random>
#include <set>
#include <string>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

// Must share major.minor with apworld/d2r_archipelago/archipelago.json.
constexpr const char* PluginVersion = "0.2.0";

constexpr D2RL::PluginInfo Info {
	.infoSize    = D2RL::PluginInfoSize,
	.abiVersion  = D2RL_PLUGIN_ABI_VERSION,
	.id          = "archipelago",
	.name        = "Archipelago",
	.version     = PluginVersion,
	.author      = "snrn",
	.description = "Archipelago multiworld client: shuffled skills arrive as a Codex charm, loot scales to your level.",
	.flags       = D2RL::PluginFlags::Shared,
};

// The Codex charm's item seed carries this marker in its top byte, then the
// chunk number, the skill level (1 + Codex Empowerments) and the skill count.
// Bump the marker whenever the Codex recipe changes, so old charms get rebuilt.
constexpr uint32_t CodexMarker     = 0xAF000000U;
constexpr uint32_t CodexMarkerMask = 0xFF000000U;
constexpr uint32_t CodexMaxSkills  = 60;
constexpr auto     FeedLifetime    = std::chrono::seconds(12);
constexpr size_t   FeedLines       = 6;

const D2RL::PluginContext*    gContext;
const D2RL::LifecycleService* gLifecycle;
const D2RL::InventoryService* gInventory;
const D2RL::ItemService*      gItems;
const D2RL::ThreadService*    gThreads;
// Optional: lets the plugin find unique and set rows by name in the live tables.
const D2RL::DataTableService* gTables;
// Optional: the Codex tooltip and the UI-message log.
const D2RL::SharedEventService* gEvents;
const D2RL::WidgetService*      gWidgets;
const D2RL::OverlayService*   gOverlay;

std::unique_ptr<ap::Client> gClient;
std::atomic<bool>           gWorkQueued;

struct FeedLine {
	std::string       text;
	Clock::time_point shown;
};

std::mutex            gMutex;
ap::Config            gConfig;
ap::SaveState         gSave;
std::filesystem::path gSavePath;
uint64_t              gBoundSession = 0;
// Checks found before the first connection, waiting for a seed to bind to.
std::set<int64_t>     gUnbound;
std::deque<FeedLine>  gFeed;
bool                  gInGame       = false;
bool                  gCodexDirty   = true;
uint64_t              gCodexItems   = UINT64_MAX;
// Quests completed by kills (server thread) or item scans (game thread), as
// difficulty * 100 + flag; the render thread sends them, off the game threads.
std::set<int>         gQuestQueue;
// Den of Evil kills per difficulty this session.
uint32_t              gDenKills[3] = {};
Clock::time_point     gNextQuestScan;

// Highest character level seen; 0 until a level-up event or a save says.
int                   gPlayerLevel  = 0;
// Where the character is, for estimating its level before that.
int                   gDifficulty   = 0;
int                   gAct          = 0;
// The player's class id seen in game (from the killer of a monster), or -1.
int                   gPlayerClass  = -1;

// D2 class ids, in order.
constexpr const char* ClassNames[] = { "Amazon", "Sorceress", "Necromancer", "Paladin", "Barbarian", "Druid", "Assassin", "Warlock" };

auto ClassIndex(const std::string& name) -> int {
	for (int i = 0; i < static_cast<int>(std::size(ClassNames)); ++i) {
		if (name == ClassNames[i]) {
			return i;
		}
	}
	return -1;
}

Clock::time_point     gLastRoomWarning;
// After a failed item update, wait before trying again instead of every frame.
Clock::time_point     gRetryAt;

void Log(const std::string& text) {
	if (gContext != nullptr) {
		gContext->LogInfo(text.c_str());
	}
}

// Messages waiting to be echoed to the D2RLoader console on the UI thread.
std::vector<std::string> gConsoleQueue;
std::atomic<bool>        gConsoleQueued;

void __cdecl FlushConsole(const D2RL::PluginContext*, void*) noexcept {
	gConsoleQueued = false;
	std::vector<std::string> lines;
	{
		std::lock_guard lock(gMutex);
		lines.swap(gConsoleQueue);
	}
	for (const auto& line : lines) {
		gContext->WriteConsoleMessage(line.c_str());
	}
}

// The latest messages, shown in the Codex's tooltip.
std::deque<std::string> gHistory;
constexpr size_t        HistoryLines = 8;

void Feed(const std::string& text) {
	std::lock_guard lock(gMutex);
	gHistory.push_back(text);
	while (gHistory.size() > HistoryLines) {
		gHistory.pop_front();
	}
	gConsoleQueue.push_back(text);
	gFeed.push_back({ text, Clock::now() });
	while (gFeed.size() > FeedLines) {
		gFeed.pop_front();
	}
}

auto ConfigText() -> std::string {
	std::string text(64 * 1024, '\0');
	uint32_t required = 0;
	if (!gContext->ReadConfig(text.data(), static_cast<uint32_t>(text.size()), &required)) {
		return {};
	}
	text.resize(std::min<size_t>(required, text.size()));
	if (const auto end = text.find('\0'); end != std::string::npos) {
		text.resize(end);
	}
	return text;
}

auto SaveDirectory() -> std::filesystem::path {
	return std::filesystem::path(gContext->scopeRootDirectory) / L"d2rloader" / L"archipelago";
}

auto SanitizeFileName(std::string name) -> std::string {
	for (char& c : name) {
		if (!std::isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '_') {
			c = '_';
		}
	}
	return name;
}

// ---- Checks ---------------------------------------------------------------

// ---- Act keys ---------------------------------------------------------------
//
// With act keys, a check in an act is recorded when you complete it but only
// sent once you hold that act's key, so progress waits on other players.

// Checks already sent this session, so key arrivals don't resend everything.
std::set<int64_t> gSent;
bool              gGoalSent = false;
// Acts we've already said are locked this session.
std::set<int>     gWarnedActs;
uint64_t          gKeyItems = UINT64_MAX;

constexpr const char* ActNames[] = { "Act I", "Act II", "Act III", "Act IV", "Act V" };
constexpr const char* DifficultyNames[] = { "Normal", "Nightmare", "Hell" };

auto ActLabel(int act) -> std::string {
	return std::string(DifficultyNames[act / 5]) + " " + ActNames[act % 5];
}

// The act (0 = Normal Act I ... 14 = Hell Act V) a location belongs to, or -1
// for checks that aren't tied to an act (level milestones).
auto ActOfLocation(int64_t location) -> int {
	const int64_t offset = location - ap::ids::LocationBase;
	if (offset >= 0 && offset < 300) {
		for (const auto& quest : ap::data::Quests) {
			if (quest.flag == offset % 100) {
				return static_cast<int>(offset / 100) * 5 + quest.act;
			}
		}
	} else if (offset >= 1000 && offset < 2000) {
		for (const auto& area : ap::data::Areas) {
			if (area.id == offset - 1000) {
				return area.act;
			}
		}
	}
	return -1;
}

auto UnlockedActs() -> std::set<int> {
	std::set<int> acts { 0 };
	for (const auto& item : gClient->Items()) {
		if (ap::ids::IsKeyItem(item.item)) {
			acts.insert(static_cast<int>(item.item - ap::ids::KeyBase));
		}
	}
	return acts;
}

// Sends every recorded check (and the goal) that isn't held behind a key.
void SendReady() {
	if (!gClient || gClient->State() != ap::ConnectionState::Connected) {
		return;
	}
	const ap::SlotData slots = gClient->Slots();
	const std::set<int64_t> server = gClient->Checked();
	const std::set<int> unlocked = UnlockedActs();
	auto open = [&](int act) { return !slots.actKeys || act <= 0 || unlocked.contains(act); };
	std::vector<int64_t> ready;
	bool goal = false;
	{
		std::lock_guard lock(gMutex);
		if (gBoundSession == 0) {
			return;
		}
		for (const int64_t check : gSave.checks) {
			if (!server.contains(check) && !gSent.contains(check) && open(ActOfLocation(check))) {
				ready.push_back(check);
				gSent.insert(check);
			}
		}
		goal = gSave.goalReached && !gGoalSent && open(slots.goal * 5 + slots.goalAct - 1);
		gGoalSent = gGoalSent || goal;
	}
	gClient->SendChecks(ready);
	if (goal) {
		gClient->SendGoal();
		Feed("Archipelago: goal complete!");
	}
}

void AddChecks(const std::vector<int64_t>& locations) {
	bool added = false;
	std::vector<int> locked;
	const ap::SlotData slots = gClient ? gClient->Slots() : ap::SlotData {};
	const std::set<int> unlocked = gClient ? UnlockedActs() : std::set<int> { 0 };
	{
		std::lock_guard lock(gMutex);
		for (const int64_t location : locations) {
			if (gBoundSession != 0 ? gSave.checks.insert(location).second : gUnbound.insert(location).second) {
				added = true;
				const int act = ActOfLocation(location);
				if (slots.actKeys && act > 0 && !unlocked.contains(act) && gWarnedActs.insert(act).second) {
					locked.push_back(act);
				}
			}
		}
		if (added && gBoundSession != 0) {
			gSave.Save(gSavePath);
		}
	}
	for (const int act : locked) {
		Feed("Archipelago: " + ActLabel(act) + " is locked; its checks are held until you find the " + ActLabel(act) + " Key.");
	}
	if (added) {
		SendReady();
	}
}

void ReachGoal() {
	{
		std::lock_guard lock(gMutex);
		gSave.goalReached = true;
		if (gBoundSession != 0) {
			gSave.Save(gSavePath);
		}
	}
	SendReady();
	bool sent = false;
	{
		std::lock_guard lock(gMutex);
		sent = gGoalSent;
	}
	if (!sent) {
		Feed("Archipelago: goal reached! It counts once you hold the key for its act.");
	}
}

void CompleteQuest(uint32_t difficulty, uint32_t flag) {
	if (difficulty >= 3) {
		return;
	}
	for (const auto& quest : ap::data::Quests) {
		if (quest.flag == flag) {
			AddChecks({ ap::ids::QuestLocation(difficulty, quest.flag) });
			const ap::SlotData slots = gClient->Slots();
			if (static_cast<int>(quest.flag) == slots.goalQuest && static_cast<int>(difficulty) == slots.goal) {
				ReachGoal();
			}
		}
	}
}

void CheckLevelMilestones(int level) {
	const ap::SlotData slots = gClient->Slots();
	if (!slots.levelChecks || slots.levelStep <= 0) {
		return;
	}
	std::vector<int64_t> reached;
	for (int milestone = slots.levelStep; milestone <= std::min(level, slots.levelCap); milestone += slots.levelStep) {
		reached.push_back(ap::ids::LevelLocation(static_cast<uint32_t>(milestone)));
	}
	AddChecks(reached);
}

// The game reads its tables at startup, so the skill lock and the seed's
// shuffles only apply if d2r-ap-setup wrote them for this seed beforehand.
// d2r-ap-setup leaves archipelago.txt (seed, slot, settings) in the mod folder.
void CheckInstalledMod(const std::string& seed) {
	const std::filesystem::path directory = gContext->modDirectory != nullptr ? gContext->modDirectory : L"";
	std::vector<std::string> stamp;
	for (const auto& candidate : { directory / L"archipelago.txt", directory.parent_path() / L"archipelago.txt" }) {
		std::ifstream file(candidate);
		for (std::string line; file && std::getline(file, line);) {
			stamp.push_back(line);
		}
		if (!stamp.empty()) {
			break;
		}
	}
	const ap::randomizer::Settings settings = gClient->Slots().randomizer;
	if (directory.empty() || stamp.empty()) {
		Feed("Archipelago: the Archipelago mod isn't running, so the skill trees aren't locked. Run d2r-ap-setup and start D2R with it.");
	} else if (settings.Any() && (stamp[0] != seed || stamp.size() < 3 || stamp[2] != settings.Stamp())) {
		Feed("Archipelago: the Archipelago mod was set up for another seed. Quit, run d2r-ap-setup, and restart.");
	}
}

// Seeds from a different apworld version may use ids or settings this plugin
// doesn't know, so say so instead of misbehaving quietly.
void CheckVersion() {
	const std::string world = gClient->Slots().worldVersion;
	auto majorMinor = [](const std::string& version) { return version.substr(0, version.rfind('.')); };
	if (world.empty()) {
		Feed("Archipelago: this seed doesn't say which apworld made it; it may be older than plugin " + std::string(PluginVersion) + ".");
	} else if (majorMinor(world) != majorMinor(PluginVersion)) {
		Feed("Archipelago: this seed was made with apworld " + world + " but the plugin is " + PluginVersion + "; update the older one.");
	}
}

// Load the save for a newly connected seed and send what the server is missing.
void BindSession(uint64_t session) {
	const std::string seed = gClient->Seed();
	const std::string slot = gClient->Slot();
	const std::set<int64_t> serverChecked = gClient->Checked();
	size_t unsent = 0;
	int level = 0;
	{
		std::lock_guard lock(gMutex);
		gSave = {};
		gSavePath = SaveDirectory() / (SanitizeFileName(seed) + "_" + SanitizeFileName(slot) + ".txt");
		gSave.Load(gSavePath);
		gSave.checks.insert(gUnbound.begin(), gUnbound.end());
		gUnbound.clear();
		gSave.checks.insert(serverChecked.begin(), serverChecked.end());
		gSave.Save(gSavePath);
		for (const int64_t check : gSave.checks) {
			unsent += serverChecked.contains(check) ? 0 : 1;
		}
		gSent.clear();
		gGoalSent = false;
		gWarnedActs.clear();
		gKeyItems = UINT64_MAX;
		gPlayerLevel = std::max(gPlayerLevel, gSave.level);
		gSave.level = gPlayerLevel;
		if (gPlayerClass < 0) {
			gPlayerClass = gSave.playerClass;
		}
		gSave.playerClass = gPlayerClass;
		level = gPlayerLevel;
		gBoundSession = session;
		gCodexDirty = true;
	}
	SendReady();
	if (level > 0) {
		CheckLevelMilestones(level);
	}
	Log("Archipelago: bound to seed " + seed + " slot " + slot + ", " + std::to_string(unsent) + " checks not on the server yet.");
	CheckInstalledMod(seed);
	CheckVersion();
}

// ---- Items (game thread) --------------------------------------------------

auto MakeSpec(uint32_t code, D2RL::Items::Quality quality, D2RL::Items::ItemContainer container) -> D2RL::Items::ItemCreateSpec {
	return {
		.structSize      = D2RL::Items::ItemCreateSpecSize,
		.code            = code,
		.quality         = quality,
		.qualityRecordId = D2RL::Items::RandomQualityRecord,
		.itemLevel       = 1,
		.seedMode        = D2RL::Items::SeedMode::Random,
		.quantity        = D2RL::Items::DefaultValue,
		.durability      = D2RL::Items::DefaultValue,
		.stateFlags      = D2RL::Items::ItemStateIdentified,
		.destination     = {
			.structSize = D2RL::Items::ItemDestinationSize,
			.container  = container,
			.placement  = D2RL::Items::Placement::Automatic,
		},
	};
}

// ---- Live table lookup (game thread) ---------------------------------------
//
// D2R's record numbers are positions in its compiled UniqueItems/SetItems
// tables, which don't always line up with the excel files (divider rows,
// mods). Compiled rows hold the item's name, so the plugin finds rows by name:
// the name's offset is learned from the first row, whose name is known.

struct NamedRows {
	uint64_t revision = UINT64_MAX;
	std::unordered_map<std::string, uint32_t> byName;
};

std::map<std::pair<uint32_t, uint32_t>, NamedRows> gNamedRows;

auto IndexRows(D2RL::DataTables::Bank bank, D2RL::DataTables::TableId table, const char* firstName) -> const NamedRows* {
	if (gTables == nullptr) {
		return nullptr;
	}
	D2RL::DataTables::TableView view { .structSize = D2RL::DataTables::TableViewSize };
	if (gTables->getTable(gContext, bank, table, &view) != D2RL::DataTables::Result::Success || view.rows == nullptr || view.rowCount == 0) {
		return nullptr;
	}
	auto& cache = gNamedRows[{ static_cast<uint32_t>(bank), static_cast<uint32_t>(table) }];
	if (cache.revision == view.revision) {
		return &cache;
	}
	cache = { view.revision, {} };
	const auto* rows = static_cast<const uint8_t*>(view.rows);
	const size_t nameLength = std::strlen(firstName);
	size_t offset = SIZE_MAX;
	for (size_t at = 0; at + nameLength < view.rowSize; ++at) {
		if (std::memcmp(rows + at, firstName, nameLength + 1) == 0) {
			offset = at;
			break;
		}
	}
	if (offset == SIZE_MAX) {
		Log("Archipelago: couldn't find \"" + std::string(firstName) + "\" in table " + std::to_string(static_cast<uint32_t>(table)) + ", bank "
			+ std::to_string(static_cast<uint32_t>(bank)) + ".");
		return &cache;
	}
	for (uint32_t row = 0; row < view.rowCount; ++row) {
		const auto* name = reinterpret_cast<const char*>(rows + static_cast<size_t>(row) * view.rowSize + offset);
		cache.byName.emplace(std::string(name, strnlen(name, std::min<size_t>(64, view.rowSize - offset))), row);
	}
	Log("Archipelago: table " + std::to_string(static_cast<uint32_t>(table)) + " bank " + std::to_string(static_cast<uint32_t>(bank)) + ": "
		+ std::to_string(view.rowCount) + " rows of " + std::to_string(view.rowSize) + " bytes, names at +" + std::to_string(offset) + ".");
	return &cache;
}

// Every record number the name has in the RotW and LoD banks, RotW first.
auto RecordsNamed(D2RL::DataTables::TableId table, const char* firstName, const std::string& name) -> std::vector<uint32_t> {
	std::vector<uint32_t> records;
	for (const auto bank : { D2RL::DataTables::Bank::Rotw, D2RL::DataTables::Bank::Lod }) {
		const NamedRows* rows = IndexRows(bank, table, firstName);
		if (rows == nullptr) {
			continue;
		}
		const auto found = rows->byName.find(name);
		if (found != rows->byName.end() && std::ranges::find(records, found->second) == records.end()) {
			records.push_back(found->second);
		}
	}
	return records;
}

auto UniqueRecords(const std::string& name) -> std::vector<uint32_t> {
	return RecordsNamed(D2RL::DataTables::TableId::UniqueItems, ap::data::Uniques[0].name, name);
}

auto SetRecords(const std::string& name) -> std::vector<uint32_t> {
	return RecordsNamed(D2RL::DataTables::TableId::SetItems, ap::data::SetItems[0].name, name);
}

auto CreateIn(D2RL::PlayerHandle player, const ap::loot::Pick& pick, uint32_t record, D2RL::Items::ItemContainer container) -> D2RL::Items::Result {
	auto spec = MakeSpec(pick.code, pick.quality, container);
	spec.qualityRecordId = record;
	spec.itemLevel = pick.itemLevel;
	spec.socketCount = pick.sockets;
	D2RL::ItemHandle item = D2RL::InvalidItemHandle;
	const auto result = gItems->createItem(gContext, player, &spec, &item);
	if (result == D2RL::Items::Result::Success && pick.record != D2RL::Items::RandomQualityRecord) {
		// Lets the first sessions confirm the *ID-to-record mapping.
		D2RL::Items::ItemInfo info { .structSize = D2RL::Items::ItemInfoSize };
		if (gItems->getItemInfo(gContext, item, &info) == D2RL::Items::Result::Success) {
			Log("Archipelago: created " + pick.name + " as record " + std::to_string(info.qualityRecordId) + " (asked for " + std::to_string(record) + ").");
		}
	}
	return result;
}

// Puts a loot item in the inventory, falling back to the cube, stash and
// ground. If D2R rejects a forced unique or set record, it retries with any
// record that fits the base. Returns false only when nothing could be placed.
bool GiveLoot(D2RL::PlayerHandle player, const ap::loot::Pick& pick) {
	std::vector<uint32_t> records;
	if (pick.quality == D2RL::Items::Quality::Unique) {
		records = UniqueRecords(pick.name);
	} else if (pick.quality == D2RL::Items::Quality::Set) {
		records = SetRecords(pick.name);
	}
	for (const auto container : { D2RL::Items::ItemContainer::Inventory, D2RL::Items::ItemContainer::Cube, D2RL::Items::ItemContainer::PersonalStash,
		     D2RL::Items::ItemContainer::Ground }) {
		auto result = D2RL::Items::Result::NotFound;
		for (const uint32_t record : records) {
			result = CreateIn(player, pick, record, container);
			if (result == D2RL::Items::Result::Success) {
				return true;
			}
		}
		result = CreateIn(player, pick, pick.record, container);
		if (result != D2RL::Items::Result::Success && pick.record != D2RL::Items::RandomQualityRecord) {
			Log("Archipelago: record " + std::to_string(pick.record) + " (" + pick.name + ") was rejected; letting D2R pick one for the base.");
			result = CreateIn(player, pick, D2RL::Items::RandomQualityRecord, container);
		}
		if (result == D2RL::Items::Result::Success) {
			return true;
		}
	}
	return false;
}

// The Junk Backpack trap: fill every free inventory cell with white junk.
auto GiveJunk(D2RL::PlayerHandle player, std::mt19937& random) -> int {
	int placed = 0;
	for (; placed < 40; ++placed) {
		const auto junk = ap::loot::Roll(ap::loot::Loot::JunkBackpack, 1, random);
		if (CreateIn(player, junk, D2RL::Items::RandomQualityRecord, D2RL::Items::ItemContainer::Inventory) != D2RL::Items::Result::Success) {
			break;
		}
	}
	return placed;
}

auto EffectiveLevel() -> int {
	std::lock_guard lock(gMutex);
	return gPlayerLevel > 0 ? gPlayerLevel : ap::loot::EstimateLevel(gDifficulty, gAct);
}

struct CodexScan {
	uint32_t code;
	std::vector<D2RL::Items::ItemInfo> found;
};

// Markers 0xA9-0xAE were earlier recipes.
auto IsCodexSeed(uint32_t seed) -> bool {
	const uint32_t marker = seed & CodexMarkerMask;
	return marker == CodexMarker || (marker >= 0xA9000000U && marker <= 0xAE000000U);
}

// The Codex seed, from whichever seed field kept it (0 if neither did).
auto CodexSeedOf(const D2RL::Items::ItemInfo& item) -> uint32_t {
	return IsCodexSeed(item.itemSeed) ? item.itemSeed : IsCodexSeed(item.generationSeed) ? item.generationSeed : 0;
}

auto DescribeItem(const D2RL::Items::ItemInfo& item) -> std::string {
	char text[200] {};
	std::snprintf(text, sizeof(text), "container=%u quality=%u itemSeed=%08X generationSeed=%08X prefix=%u suffix=%u", static_cast<uint32_t>(item.container),
		static_cast<uint32_t>(item.quality), item.itemSeed, item.generationSeed, item.prefixIds[0], item.suffixIds[0]);
	return text;
}

auto __cdecl CollectCodex(const D2RL::PluginContext*, const D2RL::Items::ItemInfo* item, void* userData) noexcept -> D2RL::Inventory::IterationAction {
	auto* scan = static_cast<CodexScan*>(userData);
	if (item == nullptr || item->structSize < D2RL::Items::ItemInfoRequiredSize || item->code != scan->code) {
		return D2RL::Inventory::IterationAction::Continue;
	}
	if (CodexSeedOf(*item) != 0) {
		scan->found.push_back(*item);
	}
	return D2RL::Inventory::IterationAction::Continue;
}

// Once per session, after a failed Codex create: tries variations of the spec
// and logs which ones D2R accepts, then removes the test items.
void DiagnoseCodex(D2RL::PlayerHandle player, const D2RL::Items::ItemCreateSpec& codex) {
	static bool diagnosed = false;
	if (diagnosed) {
		return;
	}
	diagnosed = true;
	struct Variant {
		const char* name;
		D2RL::Items::Quality quality;
		bool deterministic;
		bool properties;
	};
	static constexpr Variant Variants[] = {
		{ "magic, fixed seed, oskills", D2RL::Items::Quality::Magic, true, true },
		{ "magic, random seed, oskills", D2RL::Items::Quality::Magic, false, true },
		{ "magic, fixed seed, no properties", D2RL::Items::Quality::Magic, true, false },
		{ "magic, random seed, no properties", D2RL::Items::Quality::Magic, false, false },
		{ "normal, fixed seed, oskills", D2RL::Items::Quality::Normal, true, true },
		{ "normal, random seed, oskills", D2RL::Items::Quality::Normal, false, true },
		{ "normal, random seed, no properties", D2RL::Items::Quality::Normal, false, false },
	};
	const D2RL::Items::PropertySpec single = codex.propertyCount > 0 ? codex.properties[0] : D2RL::Items::PropertySpec {};
	for (const auto& variant : Variants) {
		auto spec = codex;
		spec.quality = variant.quality;
		if (!variant.deterministic) {
			spec.seedMode = D2RL::Items::SeedMode::Random;
			spec.generationSeed = 0;
			spec.itemSeed = 0;
		}
		spec.propertyCount = variant.properties ? 1 : 0;
		spec.properties = variant.properties ? &single : nullptr;
		D2RL::ItemHandle item = D2RL::InvalidItemHandle;
		const auto result = gItems->createItem(gContext, player, &spec, &item);
		Log(std::string("Archipelago: Codex test (") + variant.name + "): result " + std::to_string(static_cast<uint32_t>(result)) + ".");
		if (result == D2RL::Items::Result::Success) {
			gItems->destroyItem(gContext, player, item, D2RL::Items::SocketedItemPolicy::DestroyContents);
		}
	}
}

// marker (8) | class + 1 (4) | chunk (4) | fingerprint of the chunk's skills and levels (16)
auto CodexSeed(uint32_t chunk, int playerClass, uint32_t fingerprint) -> uint32_t {
	return CodexMarker | ((static_cast<uint32_t>(playerClass + 1) & 0xF) << 20) | ((chunk & 0xF) << 16) | (fingerprint & 0xFFFF);
}

// Make sure each 60-skill chunk has exactly one up-to-date Codex charm in the
// inventory. Returns false when an update couldn't be placed.
bool SyncCodex(D2RL::PlayerHandle player, const std::vector<ap::codex::Entry>& skills, int playerClass, const ap::Config& config) {
	CodexScan scan { ap::loot::CodeFromText(config.codexItem.c_str()), {} };
	const D2RL::Inventory::ItemFilter filter {
		.structSize    = D2RL::Inventory::ItemFilterSize,
		.containerMask = D2RL::Items::AllItemContainers,
	};
	if (gInventory->forEachInventoryItem(gContext, player, &filter, CollectCodex, &scan) != D2RL::Inventory::Result::Success) {
		return false;
	}
	const uint32_t chunks = static_cast<uint32_t>((skills.size() + CodexMaxSkills - 1) / CodexMaxSkills);
	bool ok = true;
	for (uint32_t chunk = 0; chunk < std::max<uint32_t>(chunks, 1); ++chunk) {
		const size_t first = chunk * CodexMaxSkills;
		const size_t count = first < skills.size() ? std::min<size_t>(CodexMaxSkills, skills.size() - first) : 0;
		const uint32_t wanted = CodexSeed(chunk, playerClass, ap::codex::Fingerprint(skills, first, count, playerClass));
		bool current = false;
		std::vector<D2RL::Items::TransactionInput> stale;
		for (const auto& item : scan.found) {
			const uint32_t seed = CodexSeedOf(item);
			if (((seed >> 16) & 0xF) != chunk && (seed & CodexMarkerMask) == CodexMarker) {
				continue;
			}
			if (!current && seed == wanted && item.container == D2RL::Items::ItemContainer::Inventory) {
				current = true;
			} else {
				stale.push_back({ item.handle, 1, D2RL::Items::SocketedItemPolicy::RejectIfNotEmpty });
			}
		}
		if (current && stale.empty()) {
			continue;
		}
		std::vector<D2RL::Items::PropertySpec> properties;
		for (size_t index = first; index < first + count; ++index) {
			// oskill works for every class. D2 adds 6 to the charm's level requirement only for skills
			// of another class than the character holding it, so your own class's skills need level 0.
			properties.push_back(D2RL::Items::MakeExactProperty(config.oskillProperty, skills[index].level, static_cast<int32_t>(skills[index].skill)));
		}
		std::vector<D2RL::Items::ItemCreateSpec> outputs;
		if (!current && count > 0) {
			auto spec = MakeSpec(scan.code, D2RL::Items::Quality::Unique, D2RL::Items::ItemContainer::Inventory);
			spec.qualityRecordId = config.codexUnique;
			// Each replacement Codex is the same unique row.
			spec.flags = D2RL::Items::ItemCreateFlagBit(D2RL::Items::ItemCreateFlag::AllowDuplicateUnique);
			spec.seedMode = D2RL::Items::SeedMode::Deterministic;
			spec.generationSeed = wanted;
			spec.itemSeed = wanted;
			spec.propertyCount = static_cast<uint32_t>(properties.size());
			spec.properties = properties.data();
			outputs.push_back(spec);
		}
		if (outputs.empty() && stale.empty()) {
			continue;
		}
		D2RL::ItemHandle createdCodex = D2RL::InvalidItemHandle;
		auto apply = [&]() {
			if (stale.empty()) {
				// Nothing to swap out: a plain create (transactions need an input).
				return gItems->createItem(gContext, player, &outputs[0], &createdCodex);
			}
			std::vector<D2RL::ItemHandle> created(outputs.size(), D2RL::InvalidItemHandle);
			const D2RL::Items::Transaction transaction {
				.structSize     = D2RL::Items::TransactionSize,
				.player         = player,
				.inputCount     = static_cast<uint32_t>(stale.size()),
				.outputCount    = static_cast<uint32_t>(outputs.size()),
				.inputs         = stale.data(),
				.outputs        = outputs.empty() ? nullptr : outputs.data(),
				.outputItems    = created.empty() ? nullptr : created.data(),
				.outputCapacity = static_cast<uint32_t>(created.size()),
			};
			D2RL::Items::TransactionResult result { .structSize = D2RL::Items::TransactionResultSize };
			const auto status = gItems->executeTransaction(gContext, &transaction, &result);
			createdCodex = created.empty() ? D2RL::InvalidItemHandle : created[0];
			return status;
		};
		auto status = D2RL::Items::Result::NotFound;
		if (!outputs.empty()) {
			for (const uint32_t record : UniqueRecords("Archipelago Codex")) {
				outputs[0].qualityRecordId = record;
				status = apply();
				if (status == D2RL::Items::Result::Success) {
					break;
				}
			}
			if (status != D2RL::Items::Result::Success) {
				outputs[0].qualityRecordId = config.codexUnique;
				status = apply();
			}
		} else {
			status = apply();
		}
		if (status != D2RL::Items::Result::Success && !outputs.empty()) {
			// No Codex unique (the mod wasn't set up, or a Classic/LoD character):
			// a Magic charm with a fixed affix that has no level requirement.
			Log("Archipelago: the Codex unique " + std::to_string(config.codexUnique) + " was refused (result " + std::to_string(static_cast<uint32_t>(status))
				+ "); making a Magic charm instead.");
			outputs[0].quality = D2RL::Items::Quality::Magic;
			outputs[0].qualityRecordId = D2RL::Items::RandomQualityRecord;
			outputs[0].flags = 0;
			outputs[0].prefixIds[0] = config.codexPrefix;
			status = apply();
		}
		if (status != D2RL::Items::Result::Success && !outputs.empty() && outputs[0].prefixIds[0] != 0) {
			Log("Archipelago: the Codex affix " + std::to_string(outputs[0].prefixIds[0]) + " was refused; letting D2R pick one.");
			outputs[0].prefixIds[0] = 0;
			status = apply();
		}
		if (status != D2RL::Items::Result::Success) {
			Log("Archipelago: Codex update failed with result " + std::to_string(static_cast<uint32_t>(status)) + ".");
			if (!outputs.empty()) {
				DiagnoseCodex(player, outputs[0]);
			}
			ok = false;
		} else if (!outputs.empty()) {
			Log("Archipelago: Codex " + std::to_string(chunk + 1) + " now holds " + std::to_string(count) + " skills.");
			D2RL::Items::ItemInfo info { .structSize = D2RL::Items::ItemInfoSize };
			if (createdCodex != D2RL::InvalidItemHandle && gItems->getItemInfo(gContext, createdCodex, &info) == D2RL::Items::Result::Success) {
				Log("Archipelago: created Codex " + DescribeItem(info) + ".");
			}
		}
	}
	return ok;
}

auto __cdecl CollectQuestItems(const D2RL::PluginContext*, const D2RL::Items::ItemInfo* item, void* userData) noexcept -> D2RL::Inventory::IterationAction {
	if (item != nullptr && item->structSize >= D2RL::Items::ItemInfoRequiredSize) {
		if (const auto flag = ap::quests::Find(ap::quests::Trigger::Item, item->code)) {
			static_cast<std::set<uint8_t>*>(userData)->insert(*flag);
		}
	}
	return D2RL::Inventory::IterationAction::Continue;
}

// Quest items anywhere on the character (inventory, cube, stash, cursor).
void ScanQuestItems(D2RL::PlayerHandle player) {
	std::set<uint8_t> flags;
	const D2RL::Inventory::ItemFilter filter { .structSize = D2RL::Inventory::ItemFilterSize, .containerMask = D2RL::Items::AllItemContainers };
	if (gInventory->forEachInventoryItem(gContext, player, &filter, CollectQuestItems, &flags) != D2RL::Inventory::Result::Success) {
		return;
	}
	std::lock_guard lock(gMutex);
	for (const uint8_t flag : flags) {
		gQuestQueue.insert(std::clamp(gDifficulty, 0, 2) * 100 + flag);
	}
}

void __cdecl GameWork(const D2RL::PluginContext*, void*) noexcept {
	gWorkQueued = false;
	D2RL::PlayerHandle player = D2RL::InvalidPlayerHandle;
	if (gInventory->getLocalPlayer(gContext, &player) != D2RL::Inventory::Result::Success) {
		return;
	}
	const uint64_t generation = gClient->ItemsGeneration();
	const std::vector<ap::NetworkItem> items = gClient->Items();
	ap::Config config;
	size_t delivered = 0;
	{
		std::lock_guard lock(gMutex);
		if (gBoundSession == 0) {
			return;
		}
		config = gConfig;
		delivered = gSave.fillersDelivered;
	}

	ScanQuestItems(player);
	{
		// After a failed item update, only scan until the retry pause is over.
		std::lock_guard lock(gMutex);
		if (Clock::now() < gRetryAt) {
			return;
		}
	}
	const auto skills = ap::codex::Entries(items, EffectiveLevel(), gClient->Slots().skillScaling);
	int playerClass = -1;
	{
		std::lock_guard lock(gMutex);
		playerClass = gPlayerClass;
	}
	if (playerClass < 0) {
		playerClass = ClassIndex(gClient->Slots().characterClass);
	}
	const bool codexOk = SyncCodex(player, skills, playerClass, config);

	std::mt19937 random(static_cast<uint32_t>(Clock::now().time_since_epoch().count()));
	const int characterLevel = EffectiveLevel();
	size_t next = std::min(delivered, items.size());
	for (; next < items.size(); ++next) {
		if (!ap::ids::IsLootItem(items[next].item)) {
			continue;
		}
		const auto loot = static_cast<ap::loot::Loot>(items[next].item - ap::ids::LootBase);
		if (loot == ap::loot::Loot::JunkBackpack) {
			Feed("Archipelago: a Junk Backpack! " + std::to_string(GiveJunk(player, random)) + " pieces of junk in your inventory.");
			continue;
		}
		const auto pick = ap::loot::Roll(loot, characterLevel, random);
		if (pick.code == 0) {
			continue;
		}
		if (!GiveLoot(player, pick)) {
			Log("Archipelago: couldn't place " + pick.name + "; will retry.");
			break;
		}
		if (pick.quality == D2RL::Items::Quality::Unique || pick.quality == D2RL::Items::Quality::Set || loot == ap::loot::Loot::HighRune) {
			Feed("Archipelago: you got " + pick.name + "!");
		}
	}

	std::lock_guard lock(gMutex);
	if (next != delivered) {
		gSave.fillersDelivered = next;
		gSave.Save(gSavePath);
	}
	if (codexOk && next == items.size()) {
		gCodexItems = generation;
		gCodexDirty = false;
	} else {
		gRetryAt = Clock::now() + std::chrono::seconds(30);
	}
	if (!codexOk && Clock::now() - gLastRoomWarning > std::chrono::seconds(30)) {
		gLastRoomWarning = Clock::now();
		gFeed.push_back({ "Archipelago: couldn't place the Codex charm; make room in your inventory (details in the log).", Clock::now() });
	}
}

// ---- Overlay --------------------------------------------------------------

// Text height for this screen: the loader default is tiny at 4K, so scale
// with the screen height (about 24 px at 1080p, 45 px at 2160p).
auto TextSize(const D2RL::Overlay::Frame* frame) -> float {
	return std::max(frame->defaultTextSize, frame->screenHeight / 48.0F);
}

auto MeasureText(const D2RL::Overlay::Frame* frame, const std::string& text, float size) -> float {
	const D2RL::Overlay::MeasureTextRequest request {
		.structSize = D2RL::Overlay::MeasureTextRequestSize,
		.canvas     = frame->canvas,
		.textSize   = size,
		.text       = text.data(),
		.textLength = static_cast<uint32_t>(std::min<size_t>(text.size(), D2RL::Overlay::MaxTextBytes)),
	};
	D2RL::Overlay::TextMetrics metrics { .structSize = D2RL::Overlay::TextMetricsSize };
	return gOverlay->measureText(gContext, &request, &metrics) == D2RL::Overlay::Result::Success ? metrics.width : size * 0.55F * text.size();
}

void FillRect(const D2RL::Overlay::Frame* frame, D2RL::Overlay::Rect rect, D2RL::Overlay::Color color) {
	const D2RL::Overlay::FilledRectangleRequest request {
		.structSize = D2RL::Overlay::FilledRectangleRequestSize,
		.canvas     = frame->canvas,
		.rect       = rect,
		.color      = color,
		.rounding   = 4.0F,
	};
	gOverlay->drawFilledRectangle(gContext, &request);
}

void DrawText(const D2RL::Overlay::Frame* frame, float x, float y, const std::string& text, D2RL::Overlay::Color color) {
	const D2RL::Overlay::TextRequest request {
		.structSize = D2RL::Overlay::TextRequestSize,
		.canvas     = frame->canvas,
		.position   = { x, y },
		.color      = color,
		.textSize   = TextSize(frame),
		.text       = text.data(),
		.textLength = static_cast<uint32_t>(std::min<size_t>(text.size(), D2RL::Overlay::MaxTextBytes)),
	};
	gOverlay->drawText(gContext, &request);
}



void __cdecl OnFrame(const D2RL::PluginContext*, const D2RL::Overlay::Frame* frame, void*) noexcept {
	if (frame == nullptr || !gClient) {
		return;
	}
	const uint64_t session = gClient->SessionGeneration();
	bool bind = false;
	{
		std::lock_guard lock(gMutex);
		bind = session != 0 && session != gBoundSession;
	}
	if (bind) {
		BindSession(session);
	}
	for (const std::string& message : gClient->TakeMessages()) {
		Feed(message);
	}
	if (const uint64_t items = gClient->ItemsGeneration(); items != gKeyItems && gClient->State() == ap::ConnectionState::Connected) {
		gKeyItems = items;
		SendReady();
	}

	std::set<int> quests;
	{
		std::lock_guard lock(gMutex);
		quests.swap(gQuestQueue);
	}
	for (const int quest : quests) {
		CompleteQuest(static_cast<uint32_t>(quest / 100), static_cast<uint32_t>(quest % 100));
	}

	bool work = false;
	std::vector<FeedLine> feed;
	{
		std::lock_guard lock(gMutex);
		const bool scanDue = Clock::now() >= gNextQuestScan;
		if (scanDue) {
			gNextQuestScan = Clock::now() + std::chrono::seconds(5);
		}
		work = gInGame && gBoundSession != 0 && (scanDue || ((gCodexDirty || gCodexItems != gClient->ItemsGeneration()) && Clock::now() >= gRetryAt));
		const auto now = Clock::now();
		while (!gFeed.empty() && now - gFeed.front().shown > FeedLifetime) {
			gFeed.pop_front();
		}
		feed.assign(gFeed.begin(), gFeed.end());
	}
	bool console = false;
	{
		std::lock_guard lock(gMutex);
		console = !gConsoleQueue.empty();
	}
	if (console && !gConsoleQueued.exchange(true) && gThreads->runOnUiThread(gContext, FlushConsole, nullptr) != D2RL::Threads::Result::Success) {
		gConsoleQueued = false;
	}
	if (work && !gWorkQueued.exchange(true)) {
		if (gThreads->runOnGameThread(gContext, GameWork, nullptr) != D2RL::Threads::Result::Success) {
			gWorkQueued = false;
		}
	}

	const float size = TextSize(frame);
	const float pad = size * 0.3F;
	std::string status = "AP: " + gClient->StatusText();
	if (gClient->State() == ap::ConnectionState::Connected) {
		status = "AP: " + gClient->Slot() + " - " + std::to_string(gClient->CheckedCount()) + "/" + std::to_string(gClient->LocationCount()) + " checks";
	}
	// Status in the top-left corner.
	const float statusX = size * 0.5F, statusY = size * 0.4F;
	FillRect(frame, { statusX - pad, statusY - pad, statusX + MeasureText(frame, status, size) + pad, statusY + size + pad }, { 0.0F, 0.0F, 0.0F, 0.55F });
	DrawText(frame, statusX, statusY, status, { 0.85F, 0.75F, 0.45F, 1.0F });

	// Messages in the bottom-left, above the skill bar, like D2's chat.
	if (!feed.empty()) {
		const float lineHeight = size * 1.25F;
		const float x = size * 0.5F;
		float width = 0.0F;
		for (const auto& line : feed) {
			width = std::max(width, MeasureText(frame, line.text, size));
		}
		float y = frame->screenHeight * 0.72F - lineHeight * static_cast<float>(feed.size());
		FillRect(frame, { x - pad, y - pad, x + width + pad, y + lineHeight * static_cast<float>(feed.size()) + pad }, { 0.0F, 0.0F, 0.0F, 0.55F });
		for (const auto& line : feed) {
			DrawText(frame, x, y, line.text, { 1.0F, 1.0F, 1.0F, 1.0F });
			y += lineHeight;
		}
	}
}

// ---- Gameplay events (UI thread) --------------------------------------------

void __cdecl OnGameplayEvent(const D2RL::PluginContext*, const D2RL::Lifecycle::GameplayEvent* event, void*) noexcept {
	using Kind = D2RL::Lifecycle::GameplayEventKind;
	if (!D2RL::Lifecycle::HasGameplayEventField(event, D2RL::Lifecycle::GameplayEventRequiredSize)) {
		return;
	}
	switch (event->kind) {
		case Kind::GameJoined:
		case Kind::LocalPlayerReady: {
			std::lock_guard lock(gMutex);
			gInGame = true;
			gCodexDirty = true;
			break;
		}
		case Kind::GameLeft: {
			std::lock_guard lock(gMutex);
			gInGame = false;
			break;
		}
		case Kind::QuestCompleted: {
			Log("Archipelago: quest row " + std::to_string(event->questRecordId) + " completed on difficulty " + std::to_string(event->difficulty) + ".");
			CompleteQuest(event->difficulty, event->questRecordId);
			break;
		}
		case Kind::LevelChanged: {
			const auto level = event->currentValue;
			for (const auto& area : ap::data::Areas) {
				if (area.id == level) {
					AddChecks({ ap::ids::AreaLocation(area.id) });
				}
			}
			if (const auto flag = ap::quests::Find(ap::quests::Trigger::Area, static_cast<uint32_t>(level))) {
				int difficulty = 0;
				{
					std::lock_guard lock(gMutex);
					difficulty = event->difficulty > 0 ? static_cast<int>(event->difficulty) : gDifficulty;
				}
				Log("Archipelago: reached the area for quest " + std::to_string(*flag) + ".");
				CompleteQuest(static_cast<uint32_t>(difficulty), *flag);
			}
			break;
		}
		case Kind::ActChanged: {
			std::lock_guard lock(gMutex);
			gAct = std::max(event->currentValue, 0);
			break;
		}
		case Kind::PlayerLevelChanged: {
			{
				std::lock_guard lock(gMutex);
				gPlayerLevel = std::max(gPlayerLevel, event->currentValue);
				// Codex skills scale with character level.
				gCodexDirty = true;
				if (gBoundSession != 0 && gPlayerLevel > gSave.level) {
					gSave.level = gPlayerLevel;
					gSave.Save(gSavePath);
				}
			}
			CheckLevelMilestones(event->currentValue);
			break;
		}
		default: break;
	}
}

// ---- Codex tooltip and UI messages (UI thread) --------------------------------

// Hovering over the Codex shows the connection and the latest messages, so you
// don't have to open the console.
void __cdecl OnItemTooltip(const D2RL::PluginContext*, D2RL::SharedEvents::ItemTooltipEvent* event, void*) noexcept {
	if (event == nullptr || event->structSize < D2RL::SharedEvents::ItemTooltipEventRequiredSize || event->text == nullptr || !gClient) {
		return;
	}
	D2RL::Items::ItemInfo info { .structSize = D2RL::Items::ItemInfoSize };
	if (gItems->getItemInfo(gContext, event->item, &info) != D2RL::Items::Result::Success || CodexSeedOf(info) == 0) {
		return;
	}
	// \xC3\xBF is D2's color marker: c4 gold, c0 white, c5 grey.
	std::string text = "\xC3\xBF" "c4" "Archipelago: ";
	text += gClient->State() == ap::ConnectionState::Connected
	          ? gClient->Slot() + ", " + std::to_string(gClient->CheckedCount()) + "/" + std::to_string(gClient->LocationCount()) + " checks"
	          : gClient->StatusText();
	{
		std::lock_guard lock(gMutex);
		for (auto line = gHistory.rbegin(); line != gHistory.rend(); ++line) {
			std::string shown = *line;
			if (shown.rfind("Archipelago: ", 0) == 0) {
				shown = shown.substr(13);
			}
			text += "\n\xC3\xBF" "c5" + shown;
		}
	}
	const size_t length = std::min<size_t>(text.size(), event->capacity - 1);
	std::memcpy(event->text, text.data(), length);
	event->text[length] = '\0';
	event->length = static_cast<uint32_t>(length);
}

// Logs each distinct D2R UI message once, to find one that shows text in game.
auto __cdecl OnUiMessage(const D2RL::PluginContext*, const D2RL::SharedEvents::UiMessageEvent* event, void*) noexcept -> D2RL::SharedEvents::UiMessageAction {
	static std::set<std::string> seen;
	if (event == nullptr || event->structSize < D2RL::SharedEvents::UiMessageEventRequiredSize || event->target == nullptr || event->command == nullptr
		|| seen.size() >= 400) {
		return D2RL::SharedEvents::UiMessageAction::Continue;
	}
	const std::string key = std::string(event->target) + ":" + event->command;
	if (seen.insert(key).second) {
		const std::string text = event->text != nullptr ? std::string(event->text).substr(0, 80) : "";
		Log("Archipelago: UI message " + key + (text.empty() ? "" : " \"" + text + "\""));
	}
	return D2RL::SharedEvents::UiMessageAction::Continue;
}

// Runs on the server thread; only used to learn the current difficulty.
void __cdecl OnMonsterDeath(const D2RL::PluginContext*, const D2RL::Lifecycle::MonsterDeathEvent* event, void*) noexcept {
	if (!D2RL::Lifecycle::HasMonsterDeathEventField(event, D2RL::Lifecycle::MonsterDeathEventRequiredSize)) {
		return;
	}
	std::lock_guard lock(gMutex);
	gDifficulty = static_cast<int>(event->difficulty);
	const int difficulty = std::clamp(gDifficulty, 0, 2);
	if (const auto flag = ap::quests::Find(ap::quests::Trigger::Kill, event->monster.classId)) {
		gQuestQueue.insert(difficulty * 100 + *flag);
	}
	if (event->levelId == static_cast<int32_t>(ap::quests::DenLevel) && ++gDenKills[difficulty] == ap::quests::DenKills) {
		gQuestQueue.insert(difficulty * 100 + 1);
	}
	// A player's kill tells us the player's class (single player: it's you).
	const int killerClass = static_cast<int>(event->killer.classId);
	if (event->killer.type == D2RL::Lifecycle::UnitType::Player && killerClass >= 0 && killerClass < static_cast<int>(std::size(ClassNames))
		&& killerClass != gPlayerClass) {
		gPlayerClass = killerClass;
		gCodexDirty = true;
		if (gBoundSession != 0) {
			gSave.playerClass = killerClass;
			gSave.Save(gSavePath);
		}
	}
}

// ---- Console commands -------------------------------------------------------

auto SplitArgs(const D2RL::ConsoleCommandContext* command) -> std::vector<std::string> {
	std::vector<std::string> args;
	const std::string text(command->args != nullptr ? command->args : "", command->argsLength);
	size_t position = 0;
	while (position < text.size()) {
		const size_t start = text.find_first_not_of(' ', position);
		if (start == std::string::npos) {
			break;
		}
		const size_t end = text.find(' ', start);
		args.push_back(text.substr(start, end == std::string::npos ? std::string::npos : end - start));
		position = end == std::string::npos ? text.size() : end;
	}
	return args;
}

auto CommandConnect(D2R::Game::Client*, const D2RL::ConsoleCommandContext* command, void*) noexcept -> D2RL::ConsoleCommandResult {
	const auto args = SplitArgs(command);
	if (args.size() < 2) {
		gContext->WriteConsoleMessage("Usage: ap-connect <server[:port]> <slot> [password]");
		return D2RL::ConsoleCommandResult::InvalidArguments;
	}
	ap::Config config;
	{
		std::lock_guard lock(gMutex);
		gConfig.server = args[0];
		gConfig.slot = args[1];
		gConfig.password = args.size() > 2 ? args[2] : "";
		config = gConfig;
		gBoundSession = 0;
	}
	if (!gContext->WriteConfig(ap::FormatConfig(config).c_str())) {
		gContext->WriteConsoleWarning("Couldn't save the server and slot to the config file.");
	}
	gClient->Connect(config.server, config.slot, config.password);
	gContext->WriteConsoleMessage(("Connecting to " + config.server + " as " + config.slot + "...").c_str());
	return D2RL::ConsoleCommandResult::Handled;
}

auto CommandDisconnect(D2R::Game::Client*, const D2RL::ConsoleCommandContext*, void*) noexcept -> D2RL::ConsoleCommandResult {
	gClient->Disconnect();
	gContext->WriteConsoleMessage("Disconnected from Archipelago.");
	return D2RL::ConsoleCommandResult::Handled;
}

auto CommandStatus(D2R::Game::Client*, const D2RL::ConsoleCommandContext*, void*) noexcept -> D2RL::ConsoleCommandResult {
	size_t skills = 0;
	for (const auto& item : gClient->Items()) {
		skills += ap::ids::IsSkillItem(item.item) ? 1 : 0;
	}
	size_t local = 0;
	{
		std::lock_guard lock(gMutex);
		local = gBoundSession != 0 ? gSave.checks.size() : gUnbound.size();
	}
	const std::string text = gClient->StatusText() + " Server has " + std::to_string(gClient->Checked().size()) + "/" + std::to_string(gClient->LocationCount())
	                       + " checks; this client found " + std::to_string(local) + ". Skills received: " + std::to_string(skills) + ".";
	gContext->WriteConsoleMessage(text.c_str());
	return D2RL::ConsoleCommandResult::Handled;
}

auto CommandSay(D2R::Game::Client*, const D2RL::ConsoleCommandContext* command, void*) noexcept -> D2RL::ConsoleCommandResult {
	const std::string text(command->args != nullptr ? command->args : "", command->argsLength);
	if (text.empty()) {
		return D2RL::ConsoleCommandResult::InvalidArguments;
	}
	gClient->Say(text);
	return D2RL::ConsoleCommandResult::Handled;
}

// ap-ui <target> <command> [text]: sends a D2R UI message, for finding one that shows text.
auto CommandUi(D2R::Game::Client*, const D2RL::ConsoleCommandContext* command, void*) noexcept -> D2RL::ConsoleCommandResult {
	const auto args = SplitArgs(command);
	if (args.size() < 2 || gWidgets == nullptr) {
		gContext->WriteConsoleMessage("Usage: ap-ui <target> <command> [text]");
		return D2RL::ConsoleCommandResult::InvalidArguments;
	}
	std::string text;
	for (size_t i = 2; i < args.size(); ++i) {
		text += (i > 2 ? " " : "") + args[i];
	}
	const D2RL::Widgets::UiAction action {
		.structSize = D2RL::Widgets::UiActionSize,
		.target     = args[0].c_str(),
		.command    = args[1].c_str(),
		.text       = text.c_str(),
	};
	const auto result = gWidgets->dispatchUiAction(gContext, &action);
	gContext->WriteConsoleMessage(("ap-ui: result " + std::to_string(static_cast<uint32_t>(result))).c_str());
	return D2RL::ConsoleCommandResult::Handled;
}

auto CommandCodex(D2R::Game::Client*, const D2RL::ConsoleCommandContext*, void*) noexcept -> D2RL::ConsoleCommandResult {
	std::lock_guard lock(gMutex);
	gCodexDirty = true;
	return D2RL::ConsoleCommandResult::Handled;
}

template <typename Service, typename Check>
bool Query(const Service*& service, Check hasFields, uint32_t required) {
	return gContext->QueryService(&service) == D2RL::ServiceQueryResult::Success && hasFields(service, required);
}

} // namespace

D2RL_PLUGIN_EXPORT auto D2RLoaderGetPluginInfo() noexcept -> const D2RL::PluginInfo* {
	return &Info;
}

D2RL_PLUGIN_EXPORT auto D2RLoaderLoadPlugin(const D2RL::PluginContext* context) noexcept -> bool {
	if (context == nullptr) {
		return false;
	}
	gContext = context;
	if (!Query(gLifecycle, D2RL::HasLifecycleServiceField, D2RL::LifecycleServiceRequiredSize)
		|| !Query(gInventory, D2RL::HasInventoryServiceField, D2RL::InventoryServiceRequiredSize)
		|| !Query(gItems, D2RL::HasItemServiceField, D2RL::ItemServiceRequiredSize)
		|| !Query(gThreads, D2RL::HasThreadServiceField, D2RL::ThreadServiceRequiredSize)
		|| !Query(gOverlay, D2RL::HasOverlayServiceField, D2RL::OverlayServiceRequiredSize)) {
		context->LogError("Archipelago: a required D2RLoader service is missing; update D2RLoader.");
		return false;
	}
	if (context->QueryService(&gTables) != D2RL::ServiceQueryResult::Success || !D2RL::HasDataTableServiceField(gTables, D2RL::DataTableServiceRequiredSize)) {
		gTables = nullptr;
		context->LogWarn("Archipelago: no data table service; unique and set rows come from the generated numbers.");
	}
	if (context->QueryService(&gWidgets) != D2RL::ServiceQueryResult::Success || !D2RL::HasWidgetServiceField(gWidgets, D2RL::WidgetServiceRequiredSize)) {
		gWidgets = nullptr;
	}
	if (context->QueryService(&gEvents) == D2RL::ServiceQueryResult::Success && D2RL::HasSharedEventServiceField(gEvents, D2RL::SharedEventServiceRequiredSize)) {
		const D2RL::SharedEvents::ItemTooltipListener tooltip {
			.structSize = D2RL::SharedEvents::ItemTooltipListenerSize,
			.region     = D2RL::SharedEvents::ItemTooltipRegion::Attributes,
			.position   = D2RL::SharedEvents::ItemTooltipPosition::Bottom,
			.anchor     = D2RL::SharedEvents::ItemTooltipAnchor::None,
			.fallback   = D2RL::SharedEvents::ItemTooltipFallback::Omit,
			.callback   = OnItemTooltip,
		};
		const D2RL::SharedEvents::UiMessageListener messages {
			.structSize = D2RL::SharedEvents::UiMessageListenerSize,
			.priority   = -1000,
			.callback   = OnUiMessage,
		};
		D2RL::SharedEvents::ListenerHandle handle = D2RL::SharedEvents::InvalidHandle;
		if (gEvents->registerItemTooltipListener(context, &tooltip, &handle) != D2RL::SharedEvents::Result::Success
			|| gEvents->registerUiMessageListener(context, &messages, &handle) != D2RL::SharedEvents::Result::Success) {
			context->LogWarn("Archipelago: couldn't register the Codex tooltip or the UI-message log.");
		}
	}
	gConfig = ap::ParseConfig(ConfigText());
	gClient = std::make_unique<ap::Client>(Log);

	using Kind = D2RL::Lifecycle::GameplayEventKind;
	for (const auto kind : { Kind::GameJoined, Kind::GameLeft, Kind::LocalPlayerReady, Kind::ActChanged, Kind::LevelChanged, Kind::PlayerLevelChanged,
		     Kind::QuestCompleted }) {
		const D2RL::Lifecycle::GameplayEventListener listener {
			.structSize = D2RL::Lifecycle::GameplayEventListenerSize,
			.kind       = kind,
			.callback   = OnGameplayEvent,
		};
		D2RL::Lifecycle::ListenerHandle handle = D2RL::Lifecycle::InvalidHandle;
		if (gLifecycle->registerGameplayEventListener(context, &listener, &handle) != D2RL::Lifecycle::Result::Success) {
			context->LogError("Archipelago: couldn't register a gameplay listener.");
			return false;
		}
	}
	const D2RL::Lifecycle::MonsterDeathListener deathListener {
		.structSize = D2RL::Lifecycle::MonsterDeathListenerSize,
		.callback   = OnMonsterDeath,
	};
	D2RL::Lifecycle::ListenerHandle deathHandle = D2RL::Lifecycle::InvalidHandle;
	if (gLifecycle->registerMonsterDeathListener(context, &deathListener, &deathHandle) != D2RL::Lifecycle::Result::Success) {
		context->LogError("Archipelago: couldn't register the monster-death listener.");
		return false;
	}
	const D2RL::Overlay::CallbackRegistration frame {
		.structSize = D2RL::Overlay::CallbackRegistrationSize,
		.phase      = D2RL::Overlay::Phase::AfterGameUi,
		.callback   = OnFrame,
	};
	D2RL::Overlay::CallbackHandle frameHandle = D2RL::Overlay::InvalidCallbackHandle;
	if (gOverlay->registerFrameCallback(context, &frame, &frameHandle) != D2RL::Overlay::Result::Success) {
		context->LogError("Archipelago: couldn't register the overlay.");
		return false;
	}

	const bool commands = context->RegisterConsoleCommand("ap-connect", CommandConnect, "Connect to an Archipelago server: ap-connect <server[:port]> <slot> [password].")
	                   && context->RegisterConsoleCommand("ap-disconnect", CommandDisconnect, "Disconnect from the Archipelago server.")
	                   && context->RegisterConsoleCommand("ap-status", CommandStatus, "Show the Archipelago connection and progress.")
	                   && context->RegisterConsoleCommand("ap-say", CommandSay, "Send a chat message to the Archipelago server.")
	                   && context->RegisterConsoleCommand("ap-codex", CommandCodex, "Rebuild the Archipelago Codex charm.")
	                   && context->RegisterConsoleCommand("ap-ui", CommandUi, "Send a D2R UI message: ap-ui <target> <command> [text].");
	if (!commands) {
		context->LogError("Archipelago: couldn't register console commands.");
		return false;
	}

	if (gConfig.autoConnect && !gConfig.server.empty() && !gConfig.slot.empty()) {
		gClient->Connect(gConfig.server, gConfig.slot, gConfig.password);
	}
	context->LogInfo("Archipelago: loaded.");
	return true;
}

D2RL_PLUGIN_EXPORT void D2RLoaderUnloadPlugin() noexcept {
	gClient.reset();
}
