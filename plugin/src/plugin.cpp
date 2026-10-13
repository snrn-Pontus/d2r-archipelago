// Archipelago multiworld client for Diablo II: Resurrected on D2RLoader.
//
// Threads: gameplay events and console commands arrive on the UI thread,
// overlay frames on the render thread, item changes run on the game thread,
// and ap::Client owns a network thread. Everything shared lives under gMutex.

#include "ap_client.h"
#include "codex.h"
#include "command_panel.h"
#include "config.h"
#include "game_data.h"
#include "ids.h"
#include "loot.h"
#include "notify.h"
#include "personalize.h"
#include "quest_triggers.h"
#include "randomizer.h"
#include "repairs.h"
#include "save_state.h"
#include "sounds.h"

#include <D2RLPlugin/api.h>
#include <D2RLPlugin/core_exports.h>

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
#include <optional>
#include <random>
#include <set>
#include <span>
#include <string>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

// Must share major.minor with apworld/d2r_archipelago/archipelago.json.
constexpr const char* PluginVersion = "0.7.3";

constexpr D2RL::PluginInfo Info {
	.infoSize    = D2RL::PluginInfoSize,
	.abiVersion  = D2RL_PLUGIN_ABI_VERSION,
	.id          = "archipelago",
	.name        = "Archipelago",
	.version     = PluginVersion,
	.author      = "snrn",
	.description = "Archipelago multiworld client: shuffled skills arrive as a Codex charm, loot scales to your level.",
	// NativeHooks is for editNativeItem, which personalizes rewards.
	.flags       = D2RL::PluginFlags::Shared | D2RL::PluginFlags::NativeHooks,
};

// The Codex charm's item seed carries this marker in its top byte, then the
// chunk number, the skill level (1 + Codex Empowerments) and the skill count.
// Bump the marker whenever the Codex recipe changes, so old charms get rebuilt.
constexpr uint32_t CodexMarker     = 0xAF000000U;
constexpr uint32_t CodexMarkerMask = 0xFF000000U;
constexpr uint32_t CodexMaxSkills  = 60;

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
// Optional: blocks Town Portal and potion use while those are locked.
const D2RL::ItemInteractionService* gInteractions;
// Optional: lets ap-relearn tell which unlocked skills a respec took away.
const D2RL::GameRuleService* gGameRules;
// Optional: the command panel (F9 or ap-panel) and the debug panel (F10 or ap-debug).
const D2RL::PanelService*     gPanels;
D2RL::Panels::RegistrationHandle gPanel = D2RL::Panels::InvalidHandle;
D2RL::Panels::RegistrationHandle gDebugPanel = D2RL::Panels::InvalidHandle;
D2RL::Panels::RegistrationHandle gRepairPanel = D2RL::Panels::InvalidHandle;
ap::repairs::Offer gRepairOffer;
std::string gRepairVendorPanel;
bool gRepairOfferQueued = false;
// UI-thread-only: only our one-shot request may pass the native repair action.
bool gNativeRepairDispatch = false;

std::unique_ptr<ap::Client> gClient;
std::atomic<bool>           gWorkQueued;
// D2RLoader drops queued game work on a session change, so GameWork may never
// run; after a while the flag is treated as stale and the work queued again.
std::atomic<int64_t>        gWorkQueuedAt;

std::mutex            gMutex;
ap::Config            gConfig;
ap::SaveState         gSave;
std::filesystem::path gSavePath;
uint64_t              gBoundSession = 0;
// Checks found before the first connection, waiting for a seed to bind to.
std::set<int64_t>     gUnbound;
bool                  gInGame       = false;
bool                  gCodexDirty   = true;
uint64_t              gCodexItems   = UINT64_MAX;
// What completed a quest. Each quest logs once per source, so a playtest log
// shows whether D2RLoader's QuestCompleted event fires and how it lines up
// with the fallbacks in quest_triggers.h (issue #20).
enum class QuestSource : uint8_t { Event, Kill, Item, Area, DenKills };
constexpr const char* QuestSourceNames[] = { "the quest event", "a boss kill", "a quest item", "reaching its area", "Den of Evil kills" };
// Quests completed by kills (server thread), item scans (game thread) or the
// quest event (UI thread), as source * 1000 + difficulty * 100 + flag, in the
// order they happened; the render thread sends them, off the game threads.
// Area triggers queue difficulty 3, the one you're on once it's known.
std::vector<int>      gQuestQueue;
constexpr int         CurrentDifficulty = 3;
// Drops area triggers still waiting for the difficulty, when the game changes (gMutex held).
void DropUnresolvedAreaQuests() {
	std::erase_if(gQuestQueue, [](int quest) { return quest % 1000 / 100 == CurrentDifficulty; });
}
// Den of Evil kills per difficulty this session.
uint32_t              gDenKills[3] = {};
// Field superunique kills (server thread), as difficulty * 100 + superunique id.
std::set<int>         gSuperUniqueQueue;
// MonStats row of the first superunique copy d2r-ap-setup appended, per bank
// (the copies are the tables' last rows), or -1 while unknown. Only set when
// the installed mod was written for this seed with superunique checks.
int                   gSuperUniqueBase[2] = { -1, -1 };
bool                  gSuperUniqueMod = false;

// Secret Cow Level checks (server thread), as difficulty * 10 + check, and cow
// kills per difficulty while no save is bound (the save keeps them otherwise).
std::set<int>         gCowQueue;
int                   gCowKills[3] = {};
// Collection checks found by the item scan (game thread).
std::vector<int64_t>  gCollectionQueue;
// D2RCore console commands to run on the UI thread (gold, experience, shrines),
// and traps, which wait until the player is out of town (unless the seed says
// otherwise). A trap's commands are made when it goes off, for the area it hits.
struct QueuedTrap {
	ap::loot::Loot    loot;
	int               level = 1;
	// When an area ambush first waited for its area's monsters.
	Clock::time_point waiting {};
};
std::vector<std::string> gCommandQueue;
std::vector<QueuedTrap> gTrapQueue;
// A release can hand over a hundred items at once, and doing it all in one frame
// crashed the game (#51), so rewards are paced: a few commands per run, a few
// loot items per pass, and traps some seconds apart.
constexpr size_t      CommandsPerRun = 4;
constexpr auto        CommandGap     = std::chrono::milliseconds(250);
constexpr size_t      LootPerPass    = 3;
constexpr auto        LootPassGap    = std::chrono::seconds(1);
constexpr auto        TrapGap        = std::chrono::seconds(3);
Clock::time_point     gNextCommandsAt;
Clock::time_point     gNextTrapAt;
// The level (area) the player is in, from the area events.
int                   gCurrentLevel = -1;

auto IsTown(int level) -> bool {
	return level == 1 || level == 40 || level == 75 || level == 103 || level == 109;
}
std::atomic<bool>     gCommandsQueued;

// What's locked right now (slot data minus the lock items received), and the
// slot data, for the threads that can't ask the client. Refreshed by UpdateLocks.
struct LockState {
	bool cube = false, townPortal = false, potions = false, stash = false, merchants = false;
	bool waypoints[5] = {};
};
LockState             gLocks;
ap::SlotData          gSlots;
Clock::time_point     gLastLockMessage;
Clock::time_point     gLastActWarp;

constexpr int TownLevels[] = { 1, 40, 75, 103, 109 };

// Hints found in game, waiting to be revealed on the render thread: the line
// that introduces each ("Deckard Cain: ...").
std::vector<std::string> gHintQueue;

// DeathLink: another player died and this one hasn't been killed yet.
bool                  gDeathPending = false;
std::atomic<bool>     gKillQueued;
// When a DeathLink death killed the player, so that death isn't sent back.
Clock::time_point     gLinkKillAt;
// Set from the death until resurrection or a new game, so each death is sent once.
bool                  gPlayerDead = false;
Clock::time_point     gPlayerReadyAt;
Clock::time_point     gNextQuestScan;

// Highest character level seen; 0 until a level-up event or a save says.
int                   gPlayerLevel  = 0;
// Where the character is, for estimating its level before that.
int                   gDifficulty   = 0;
// Whether a monster death in this game has told us gDifficulty. Areas entered
// before that wait in gPendingAreas, since the area events don't say.
bool                  gDifficultyKnown = false;
std::vector<int>      gPendingAreas;
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
// After a failed item update, wait before trying again instead of every frame;
// a short wait between loot passes too.
Clock::time_point     gRetryAt;
// Tree mode: a Token or Writ of Absolution just used, waiting for its respec to finish
// so the received skills can be unlocked again.
D2RL::ItemHandle      gRespecToken = D2RL::InvalidItemHandle;
Clock::time_point     gRespecTokenAt;

void Log(const std::string& text) {
	if (gContext != nullptr) {
		gContext->LogInfo(text.c_str());
	}
}

// Messages waiting to be echoed to the D2RLoader console on the UI thread.
std::vector<std::string> gConsoleQueue;
std::atomic<bool>        gConsoleQueued;
// A release prints hundreds of lines (#51): a few go out at a time, and a
// backlog past ConsoleBacklog goes to the log with one line saying so.
constexpr size_t         LinesPerFlush  = 3;
constexpr size_t         ConsoleBacklog = 30;
constexpr auto           FlushGap       = std::chrono::milliseconds(250);
Clock::time_point        gNextFlushAt;
std::atomic<bool>        gBannerQueued;

// The overlay doesn't render, so messages also go to D2R's chat through
// D2RCore's DisplayServerChatMessage export. It isn't part of the plugin SDK:
// it takes a raw chat packet (0x26), and for type 1 from a player D2RCore
// shows "<name>: <message>" itself. Call it on the UI thread only.
using DisplayServerChatMessageFn = void*(__fastcall*)(const uint8_t* packet) noexcept;

auto ChatFunction() -> DisplayServerChatMessageFn {
	static const auto function = [] {
		const HMODULE core = GetModuleHandleW(L"D2RCore.dll");
		return core != nullptr ? reinterpret_cast<DisplayServerChatMessageFn>(GetProcAddress(core, "DisplayServerChatMessage")) : nullptr;
	}();
	return function;
}

void ShowInChat(const std::string& text) {
	const auto display = ChatFunction();
	if (display == nullptr || text.empty()) {
		return;
	}
	constexpr size_t NameOffset = 0x0A, TextOffset = 0x47, TextBytes = 0xFF;
	constexpr char   Sender[]   = "Archipelago";
	uint8_t packet[TextOffset + TextBytes + 1] = {};
	packet[0] = 0x26; // game chat
	packet[1] = 0x01; // normal chat; packet[3] = 0 means the sender is a player
	std::memcpy(packet + NameOffset, Sender, sizeof(Sender) - 1);
	std::memcpy(packet + TextOffset, text.data(), std::min(text.size(), TextBytes));
	display(packet);
}

void __cdecl FlushConsole(const D2RL::PluginContext*, void*) noexcept {
	gConsoleQueued = false;
	std::vector<std::string> lines;
	std::vector<std::string> skipped;
	bool inGame = false;
	{
		std::lock_guard lock(gMutex);
		if (Clock::now() < gNextFlushAt) {
			return;
		}
		if (gConsoleQueue.size() > ConsoleBacklog) {
			const auto drop = static_cast<std::ptrdiff_t>(gConsoleQueue.size() - ConsoleBacklog);
			skipped.assign(gConsoleQueue.begin(), gConsoleQueue.begin() + drop);
			gConsoleQueue.erase(gConsoleQueue.begin(), gConsoleQueue.begin() + drop);
		}
		const auto take = static_cast<std::ptrdiff_t>(std::min(gConsoleQueue.size(), LinesPerFlush));
		lines.assign(gConsoleQueue.begin(), gConsoleQueue.begin() + take);
		gConsoleQueue.erase(gConsoleQueue.begin(), gConsoleQueue.begin() + take);
		inGame = gInGame;
		gNextFlushAt = Clock::now() + FlushGap;
	}
	for (const auto& line : skipped) {
		Log(line);
	}
	if (!skipped.empty()) {
		lines.insert(lines.begin(), "Archipelago: " + std::to_string(skipped.size()) + " more messages are in the D2RLoader log.");
	}
	for (const auto& line : lines) {
		gContext->WriteConsoleMessage(line.c_str());
		if (inGame) {
			ShowInChat(line);
		}
	}
}

// A command's answer, on the UI thread: the console, and the chat in game, so
// answers to the command panel's buttons can be seen.
void Reply(const std::string& text) {
	gContext->WriteConsoleMessage(text.c_str());
	bool inGame = false;
	{
		std::lock_guard lock(gMutex);
		inGame = gInGame;
	}
	if (inGame) {
		ShowInChat(text);
	}
}

// Set when the Codex unique only works from the Lord of Destruction table: the
// character isn't a Reign of the Warlock one, so Warlock skills don't exist for it.
bool                  gLodCharacter = false;
std::set<uint32_t>    gWarnedWarlockSkills;

auto IsWarlockSkill(uint32_t skill) -> bool {
	for (const auto& entry : ap::data::Skills) {
		if (entry.id == skill) {
			return std::string_view(entry.cls) == "Warlock";
		}
	}
	return false;
}

auto SkillName(uint32_t skill) -> std::string {
	for (const auto& entry : ap::data::Skills) {
		if (entry.id == skill) {
			return entry.name;
		}
	}
	return "skill " + std::to_string(skill);
}

// The Archipelago Codex's unique rows, found on the game thread for the
// tooltip (item copies on the UI thread don't carry the seed marker).
std::vector<uint32_t> gCodexRecords;

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

// The game folder, whichever scope the plugin loaded from. Save state and
// sounds live under it, so moving the plugin into the mod keeps both.
auto GameDirectory() -> std::filesystem::path {
	wchar_t path[MAX_PATH] {};
	GetModuleFileNameW(nullptr, path, MAX_PATH);
	return std::filesystem::path(path).parent_path();
}

auto SaveDirectory() -> std::filesystem::path {
	return GameDirectory() / L"d2rloader" / L"archipelago";
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
auto ActOfLocation(int64_t location, const ap::SlotData& slots) -> int {
	const int64_t offset = location - ap::ids::LocationBase;
	if (offset >= 0 && offset < 300) {
		for (const auto& quest : ap::data::Quests) {
			if (quest.flag == offset % 100) {
				return static_cast<int>(offset / 100) * 5 + quest.act;
			}
		}
	} else if (offset >= 1000 && offset < 1600) {
		for (const std::span<const ap::data::Area> areas : { std::span<const ap::data::Area>(ap::data::Areas), std::span<const ap::data::Area>(ap::data::SideAreas) }) {
			for (const auto& area : areas) {
				if (area.id == (offset - 1000) % 200) {
					return static_cast<int>((offset - 1000) / 200) * 5 + area.act;
				}
			}
		}
	} else if (offset >= 2000 && offset < 2100) {
		return ap::ids::LevelAct(static_cast<int>(offset - 2000), slots.goal * 5 + slots.goalAct, slots.levelCap);
	} else if (offset >= 5400 && offset < 5430) {
		return static_cast<int>((offset - 5400) / 10) * 5 + 4;
	} else if (offset >= 5500 && offset < 5550) {
		return static_cast<int>((offset - 5500) / 10);
	} else if (offset >= 5600 && offset < 5610) {
		return 14;
	} else if (offset >= 5900 && offset < 5960) {
		const auto index = static_cast<size_t>((offset - 5900) % 20);
		return index < std::size(ap::data::QuestItems) ? static_cast<int>((offset - 5900) / 20) * 5 + ap::data::QuestItems[index].act : -1;
	} else if (offset >= 6000 && offset < 6300) {
		return static_cast<int>((offset - 6000) / 100) * 5 + static_cast<int>((offset - 6000) % 100 / 10);
	} else if (offset >= 3000 && offset < 3300) {
		for (const auto& superUnique : ap::data::SuperUniques) {
			if (superUnique.id == (offset - 3000) % 100) {
				return static_cast<int>((offset - 3000) / 100) * 5 + superUnique.act;
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
			if (!server.contains(check) && !gSent.contains(check) && open(ActOfLocation(check, slots))) {
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

// An on-screen banner, unless notifications are off in the config.
void Banner(const std::string& text, ap::notify::Style style) {
	{
		std::lock_guard lock(gMutex);
		if (!gConfig.notifications) {
			return;
		}
	}
	ap::notify::Show(text, style);
}

void AddChecks(const std::vector<int64_t>& locations) {
	bool added = false;
	std::vector<int> locked;
	std::vector<std::pair<int64_t, bool>> announce; // new checks, and whether they're held
	const ap::SlotData slots = gClient ? gClient->Slots() : ap::SlotData {};
	const std::set<int> unlocked = gClient ? UnlockedActs() : std::set<int> { 0 };
	{
		std::lock_guard lock(gMutex);
		for (const int64_t location : locations) {
			if (gBoundSession != 0 ? gSave.checks.insert(location).second : gUnbound.insert(location).second) {
				added = true;
				const int act = ActOfLocation(location, slots);
				if (gBoundSession != 0) {
					announce.emplace_back(location, slots.actKeys && act > 0 && !unlocked.contains(act));
				}
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
	// A banner per check, naming what it held for another player (your own
	// items get their own banner when they arrive).
	if (announce.size() > 4) {
		Banner("Checks sent: " + std::to_string(announce.size()), ap::notify::Style::Check);
	} else if (!announce.empty()) {
		const auto scouted = gClient->Scouted();
		const int me = gClient->MySlot();
		for (const auto& [location, held] : announce) {
			std::string text = "Check: " + gClient->LocationName(location, me);
			if (const auto found = scouted.find(location); found != scouted.end() && found->second.player != me) {
				text += " - " + gClient->ItemName(found->second.item, found->second.player) + " for " + gClient->PlayerName(found->second.player);
			}
			Banner(text + (held ? " (held)" : ""), ap::notify::Style::Check);
		}
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

// Whether a check hasn't been found yet (so its first time can earn a hint).
auto IsNewCheck(int64_t location) -> bool {
	std::lock_guard lock(gMutex);
	return gBoundSession != 0 && !gSave.checks.contains(location);
}

constexpr uint32_t BossQuestFlags[] = { 6, 14, 22, 26, 40 };
constexpr uint32_t DenOfEvilFlag    = 1;

void CompleteQuest(uint32_t difficulty, uint32_t flag, QuestSource source) {
	if (difficulty >= 3) {
		return;
	}
	for (const auto& quest : ap::data::Quests) {
		if (quest.flag == flag) {
			const int64_t location = ap::ids::QuestLocation(difficulty, quest.flag);
			// The quest event (UI thread) and the fallbacks (render thread) both get here.
			static std::set<std::pair<int64_t, QuestSource>> logged;
			bool first = false;
			{
				std::lock_guard lock(gMutex);
				first = logged.emplace(location, source).second;
			}
			if (first) {
				Log("Archipelago: " + std::string(DifficultyNames[difficulty]) + " " + quest.name + " completed by " + QuestSourceNames[static_cast<int>(source)] + ".");
			}
			// Deckard Cain has a hint after each act boss.
			if (std::ranges::find(BossQuestFlags, flag) != std::end(BossQuestFlags) && IsNewCheck(location) && gClient->Slots().cainHints) {
				std::lock_guard lock(gMutex);
				gHintQueue.push_back("Deckard Cain: \"Stay awhile and listen...\"");
			}
			// Den of Evil's reward is Akara's respec, which takes the received tree skills away.
			if (flag == DenOfEvilFlag && IsNewCheck(location) && gClient->Slots().randomizer.treeMode) {
				Feed("Archipelago: Akara's respec takes your Archipelago skills away. After it, use Relearn Skills (F9) and type ap-relearn confirm. "
				     "A Writ of Absolution from any potion seller respecs and unlocks them again by itself.");
			}
			AddChecks({ location });
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
	const bool matches = !stamp.empty() && stamp[0] == seed && stamp.size() >= 3 && stamp[2] == settings.Stamp();
	{
		std::lock_guard lock(gMutex);
		gSuperUniqueMod = settings.superUniqueChecks && matches;
	}
	if (directory.empty() || stamp.empty()) {
		Feed("Archipelago: the Archipelago mod isn't running, so the skill trees aren't locked. Run d2r-ap-setup and start D2R with it.");
	} else if (settings.Any() && !matches) {
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
		gRepairOffer.Cancel();
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

// Where the compiled Levels rows keep their monster line-ups (MonStats rows,
// 16 bits each): Normal at monsters, Nightmare and Hell at nightmare.
struct LineupLayout {
	uint64_t revision = UINT64_MAX;
	size_t   monsters = SIZE_MAX;
	size_t   nightmare = SIZE_MAX;
	uint16_t empty = 0;
	std::vector<std::string> names;
};
std::map<uint32_t, LineupLayout> gLineups;

// The layout isn't in the SDK, so it's found from the Pandemonium areas, which
// no shuffle touches: Pandemonium 1's line-up appears once for Normal, then
// again for Nightmare/Hell; Pandemonium 2's ends with an empty slot.
auto FindLineups(D2RL::DataTables::Bank bank, const D2RL::DataTables::TableView& levels) -> const LineupLayout& {
	auto& layout = gLineups[static_cast<uint32_t>(bank)];
	if (layout.revision == levels.revision) {
		return layout;
	}
	layout = { .revision = levels.revision };
	const NamedRows* monstats = IndexRows(bank, D2RL::DataTables::TableId::MonStats, "skeleton1");
	if (monstats == nullptr || levels.rowCount <= 134) {
		return layout;
	}
	const auto row = [&](const std::vector<const char*>& names) {
		std::vector<uint16_t> ids;
		for (const char* name : names) {
			const auto found = monstats->byName.find(name);
			ids.push_back(found == monstats->byName.end() ? UINT16_MAX : static_cast<uint16_t>(found->second));
		}
		return ids;
	};
	const auto pandemonium1 = row({ "succubus8", "succubuswitch9", "corruptrogue6", "cr_archer8", "cr_lancer8" });
	const auto pandemonium2 = row({ "swarm5", "sandmaggot7", "arach7", "scarab8" });
	const auto* rows = static_cast<const uint8_t*>(levels.rows);
	const auto matches = [&](uint32_t level, size_t at, const std::vector<uint16_t>& ids) {
		if (at + (ids.size() + 1) * 2 > levels.rowSize) {
			return false;
		}
		const uint8_t* cell = rows + static_cast<size_t>(level) * levels.rowSize + at;
		return std::memcmp(cell, ids.data(), ids.size() * 2) == 0;
	};
	std::vector<size_t> found;
	for (size_t at = 0; at + 2 <= levels.rowSize; at += 2) {
		if (matches(133, at, pandemonium1) && matches(134, at, pandemonium2)) {
			found.push_back(at);
		}
	}
	if (found.size() < 2) {
		Log("Archipelago: couldn't find the monster line-ups in Levels (bank " + std::to_string(static_cast<uint32_t>(bank)) + "); ambushes use the act's monsters.");
		return layout;
	}
	layout.monsters = found[0];
	layout.nightmare = found[1];
	std::memcpy(&layout.empty, rows + 134 * static_cast<size_t>(levels.rowSize) + layout.monsters + pandemonium2.size() * 2, 2);
	for (const auto& [name, index] : monstats->byName) {
		layout.names.resize(std::max<size_t>(layout.names.size(), index + 1));
		layout.names[index] = name;
	}
	Log("Archipelago: Levels line-ups at +" + std::to_string(layout.monsters) + " and +" + std::to_string(layout.nightmare) + ", empty slots "
		+ std::to_string(layout.empty) + ".");
	return layout;
}

// The MonStats ids of the monsters that live in an area on this difficulty
// (after any monster shuffle), or none if that can't be read. Game thread
// (the tables are game-thread-only).
auto AreaMonsters(int level, int difficulty, bool lod) -> std::vector<std::string> {
	if (gTables == nullptr || level <= 0) {
		return {};
	}
	const auto bank = lod ? D2RL::DataTables::Bank::Lod : D2RL::DataTables::Bank::Rotw;
	D2RL::DataTables::TableView view { .structSize = D2RL::DataTables::TableViewSize };
	if (gTables->getTable(gContext, bank, D2RL::DataTables::TableId::Levels, &view) != D2RL::DataTables::Result::Success || view.rows == nullptr
		|| static_cast<uint32_t>(level) >= view.rowCount) {
		return {};
	}
	const LineupLayout& layout = FindLineups(bank, view);
	const size_t at = difficulty > 0 ? layout.nightmare : layout.monsters;
	if (at == SIZE_MAX || at + 25 * 2 > view.rowSize) {
		return {};
	}
	std::vector<std::string> monsters;
	const auto* cells = static_cast<const uint8_t*>(view.rows) + static_cast<size_t>(level) * view.rowSize + at;
	for (size_t slot = 0; slot < 25; ++slot) {
		uint16_t id = 0;
		std::memcpy(&id, cells + slot * 2, 2);
		if (id == layout.empty || id >= layout.names.size() || layout.names[id].empty()) {
			continue;
		}
		monsters.push_back(layout.names[id]);
	}
	return monsters;
}

// The current area's monsters for area ambushes, read on the game thread for
// RunCommands. level is -1 until read.
struct AmbushArea {
	int level = -1;
	int difficulty = 0;
	std::vector<std::string> monsters;
};
AmbushArea gAmbushArea;

void ReadAmbushArea() {
	int  level = -1, difficulty = 0;
	bool lod = false, wanted = false;
	{
		std::lock_guard lock(gMutex);
		wanted = gSlots.traps.areaAmbush && std::ranges::any_of(gTrapQueue, [](const QueuedTrap& trap) { return trap.loot == ap::loot::Loot::AmbushTrap; });
		level = gCurrentLevel;
		difficulty = gDifficulty;
		lod = gLodCharacter;
		if (!wanted || (gAmbushArea.level == level && gAmbushArea.difficulty == difficulty)) {
			return;
		}
	}
	auto monsters = AreaMonsters(level, difficulty, lod);
	std::lock_guard lock(gMutex);
	gAmbushArea = { level, difficulty, std::move(monsters) };
}

// Items the plugin hands out carry this marker in their seed's top byte, so
// they don't count for collection checks.
constexpr uint32_t RewardMarker = 0xB7000000U;

auto IsReward(const D2RL::Items::ItemInfo& item) -> bool {
	return (item.itemSeed & CodexMarkerMask) == RewardMarker || (item.generationSeed & CodexMarkerMask) == RewardMarker;
}

auto DescribeItem(const D2RL::Items::ItemInfo& item) -> std::string;

// Copies memory that may not be readable, for the native item checks.
bool SafeCopy(void* to, const void* from, size_t size) noexcept {
	__try {
		std::memcpy(to, from, size);
		return true;
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

struct PersonalizeJob {
	D2RL::Items::ItemInfo info;
	char                  name[ap::personal::NameSize];
	bool                  dumpOnly;
	std::string           problem;
	std::string           dump;
};

// Set once the item data doesn't match the expected layout, so after a game
// update rewards stop writing instead of writing to the wrong place.
std::atomic<bool> gPersonalizeOff;

void __cdecl PersonalizeNative(const D2RL::PluginContext*, void* native, void* userData) noexcept {
	namespace personal = ap::personal;
	auto& job = *static_cast<PersonalizeJob*>(userData);
	uint8_t unit[personal::UnitSize] {};
	if (native == nullptr || !SafeCopy(unit, native, sizeof(unit))) {
		job.problem = "the item unit isn't readable";
		return;
	}
	uint32_t type = 0, classId = 0, unitId = 0;
	void*    data = nullptr;
	std::memcpy(&type, unit + personal::UnitType, sizeof(type));
	std::memcpy(&classId, unit + personal::UnitClassId, sizeof(classId));
	std::memcpy(&unitId, unit + personal::UnitId, sizeof(unitId));
	std::memcpy(&data, unit + personal::UnitData, sizeof(data));
	if (type != personal::ItemUnitType || classId != job.info.classId || unitId != job.info.runtimeId) {
		job.problem = "the native pointer isn't the item unit";
		job.dump = personal::HexDump(unit);
		return;
	}
	uint8_t bytes[personal::DataSize] {};
	if (data == nullptr || !SafeCopy(bytes, data, sizeof(bytes))) {
		job.problem = "the item data isn't readable";
		return;
	}
	job.dump = personal::HexDump(bytes);
	job.problem = personal::CheckLayout(bytes, static_cast<uint32_t>(job.info.quality), (job.info.stateFlags & D2RL::Items::ItemStateIdentified) != 0,
		(job.info.stateFlags & D2RL::Items::ItemStateEthereal) != 0);
	if (!job.problem.empty() || job.dumpOnly) {
		return;
	}
	uint32_t flags = 0;
	std::memcpy(&flags, bytes + personal::DataFlags, sizeof(flags));
	flags |= personal::FlagPersonalized;
	auto* target = static_cast<uint8_t*>(data);
	if (!SafeCopy(target + personal::DataName, job.name, sizeof(job.name)) || !SafeCopy(target + personal::DataFlags, &flags, sizeof(flags))) {
		job.problem = "the item data isn't writable";
	}
}

// Writes name into the item as D2's personalization (game thread). Returns why
// it didn't, or an empty string. dumpOnly checks and logs the item data only.
auto Personalize(D2RL::ItemHandle item, const std::string& name, bool dumpOnly) -> std::string {
	if (!dumpOnly && gPersonalizeOff) {
		return "personalization is off for this session (see the log)";
	}
	PersonalizeJob job { .info = { .structSize = D2RL::Items::ItemInfoSize }, .name = {}, .dumpOnly = dumpOnly };
	if (gItems->getItemInfo(gContext, item, &job.info) != D2RL::Items::Result::Success) {
		return "no item info";
	}
	std::memcpy(job.name, name.data(), std::min(name.size(), sizeof(job.name) - 1));
	const auto result = gItems->editNativeItem(gContext, item, PersonalizeNative, &job);
	if (result != D2RL::Items::Result::Success) {
		return "editNativeItem failed (result " + std::to_string(static_cast<uint32_t>(result)) + ")";
	}
	if (dumpOnly || !job.problem.empty()) {
		Log("Archipelago: item data of " + DescribeItem(job.info) + " level=" + std::to_string(job.info.itemLevel) + " record=" + std::to_string(job.info.qualityRecordId)
			+ " state=" + std::to_string(job.info.stateFlags) + (job.problem.empty() ? "" : " (" + job.problem + ")") + ":\n" + job.dump);
	}
	if (!job.problem.empty() && !dumpOnly) {
		gPersonalizeOff = true;
	}
	return job.problem;
}

// owner, when set, goes on a unique or set item as its personalization.
auto CreateIn(D2RL::PlayerHandle player, const ap::loot::Pick& pick, uint32_t record, D2RL::Items::ItemContainer container, const std::string& owner = {})
	-> D2RL::Items::Result {
	static std::mt19937 seeds(static_cast<uint32_t>(Clock::now().time_since_epoch().count()));
	auto spec = MakeSpec(pick.code, pick.quality, container);
	spec.qualityRecordId = record;
	spec.seedMode = D2RL::Items::SeedMode::Deterministic;
	spec.generationSeed = RewardMarker | (seeds() & ~CodexMarkerMask);
	spec.itemSeed = spec.generationSeed;
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
	if (result == D2RL::Items::Result::Success && !owner.empty() && (pick.quality == D2RL::Items::Quality::Unique || pick.quality == D2RL::Items::Quality::Set)) {
		if (const auto problem = Personalize(item, owner, false); !problem.empty()) {
			Log("Archipelago: couldn't put " + owner + "'s name on " + pick.name + ": " + problem + ".");
		}
	}
	return result;
}

// Puts a loot item in the inventory, falling back to the cube, stash and
// ground. A locked cube or stash is skipped, so the item can't be stored where
// the player can't reach it. If D2R rejects a forced unique or set record, it
// retries with any record that fits the base. Returns false only when nothing
// could be placed. owner, when set, is the sender's name for a unique or set item.
bool GiveLoot(D2RL::PlayerHandle player, const ap::loot::Pick& pick, const std::string& owner = {}) {
	bool cubeLocked = false, stashLocked = false;
	{
		std::lock_guard lock(gMutex);
		cubeLocked = gLocks.cube;
		stashLocked = gLocks.stash;
	}
	std::vector<uint32_t> records;
	if (pick.quality == D2RL::Items::Quality::Unique) {
		records = UniqueRecords(pick.name);
	} else if (pick.quality == D2RL::Items::Quality::Set) {
		records = SetRecords(pick.name);
	}
	// Logged with each placement, so the log shows a locked cube or stash was passed over.
	std::string skipped;
	if (cubeLocked) {
		skipped += ", cube locked";
	}
	if (stashLocked) {
		skipped += ", stash locked";
	}
	const auto placedIn = [&](D2RL::Items::ItemContainer container) {
		using enum D2RL::Items::ItemContainer;
		const char* name = container == Inventory ? "inventory"
		                   : container == Cube    ? "cube"
		                   : container == PersonalStash ? "stash"
		                   : container == Ground  ? "ground"
		                                          : "cursor";
		Log("Archipelago: placed " + pick.name + " in the " + name + (skipped.empty() ? std::string {} : " (" + skipped.substr(2) + ")") + ".");
	};
	std::string results;
	for (const auto container : { D2RL::Items::ItemContainer::Inventory, D2RL::Items::ItemContainer::Cube, D2RL::Items::ItemContainer::PersonalStash,
		     D2RL::Items::ItemContainer::Ground, D2RL::Items::ItemContainer::Cursor }) {
		if ((container == D2RL::Items::ItemContainer::Cube && cubeLocked) || (container == D2RL::Items::ItemContainer::PersonalStash && stashLocked)) {
			continue;
		}
		auto result = D2RL::Items::Result::NotFound;
		for (const uint32_t record : records) {
			result = CreateIn(player, pick, record, container, owner);
			if (result == D2RL::Items::Result::Success) {
				placedIn(container);
				return true;
			}
		}
		result = CreateIn(player, pick, pick.record, container, owner);
		if (result != D2RL::Items::Result::Success && pick.record != D2RL::Items::RandomQualityRecord) {
			Log("Archipelago: record " + std::to_string(pick.record) + " (" + pick.name + ") was rejected; letting D2R pick one for the base.");
			result = CreateIn(player, pick, D2RL::Items::RandomQualityRecord, container, owner);
		}
		if (result == D2RL::Items::Result::Success) {
			placedIn(container);
			if (container == D2RL::Items::ItemContainer::Ground || container == D2RL::Items::ItemContainer::Cursor) {
				Feed("Archipelago: your bags are full, so " + pick.name + (container == D2RL::Items::ItemContainer::Ground ? " dropped at your feet." : " is in your hand."));
			}
			return true;
		}
		results += " " + std::to_string(static_cast<uint32_t>(container)) + "=" + std::to_string(static_cast<uint32_t>(result));
	}
	Log("Archipelago: no room for " + pick.name + " (container=result:" + results + skipped + ").");
	return false;
}

// The Junk Backpack trap: fill free inventory cells with white junk, up to most pieces.
auto GiveJunk(D2RL::PlayerHandle player, int most, std::mt19937& random) -> int {
	int placed = 0;
	for (; placed < most; ++placed) {
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

// Make sure each chunk with skills has exactly one up-to-date Codex charm in the
// inventory, and the rest have none. Returns false when an update couldn't be placed.
bool SyncCodex(D2RL::PlayerHandle player, const std::vector<ap::codex::Entry>& allSkills, int playerClass, const ap::Config& config) {
	CodexScan scan { ap::loot::CodeFromText(config.codexItem.c_str()), {} };
	const D2RL::Inventory::ItemFilter filter {
		.structSize    = D2RL::Inventory::ItemFilterSize,
		.containerMask = D2RL::Items::AllItemContainers,
	};
	if (gInventory->forEachInventoryItem(gContext, player, &filter, CollectCodex, &scan) != D2RL::Inventory::Result::Success) {
		return false;
	}
	const auto chunks = ap::codex::Chunks(allSkills, playerClass >= 0 && playerClass < static_cast<int>(std::size(ClassNames)) ? ClassNames[playerClass] : "",
		CodexMaxSkills, gClient->Slots().randomizer.skillTree);
	// Charms from older recipes count as chunk 0, so each is replaced once.
	auto chunkOf = [](uint32_t seed) { return (seed & CodexMarkerMask) == CodexMarker ? (seed >> 16) & 0xF : 0U; };
	bool ok = true;
	for (uint32_t chunk = 0; chunk < ap::codex::ChunkCount; ++chunk) {
		const auto& skills = chunks[chunk];
		const size_t first = 0;
		const size_t count = skills.size();
		if (count == 0 && std::ranges::none_of(scan.found, [&](const auto& item) { return chunkOf(CodexSeedOf(item)) == chunk; })) {
			continue;
		}
		const uint32_t wanted = CodexSeed(chunk, playerClass, ap::codex::Fingerprint(skills, first, count, playerClass));
		bool current = false;
		std::vector<D2RL::Items::TransactionInput> stale;
		for (const auto& item : scan.found) {
			const uint32_t seed = CodexSeedOf(item);
			if (chunkOf(seed) != chunk) {
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
			// RotW row first, then LoD: which one works tells us the character's expansion.
			const auto records = UniqueRecords("Archipelago Codex");
			for (size_t i = 0; i < records.size(); ++i) {
				outputs[0].qualityRecordId = records[i];
				status = apply();
				if (status == D2RL::Items::Result::Success) {
					const bool lod = records.size() > 1 && i == records.size() - 1;
					std::lock_guard lock(gMutex);
					if (lod != gLodCharacter) {
						gLodCharacter = lod;
						gCodexDirty = true;
					}
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
			Log("Archipelago: Codex " + std::to_string(chunk + 1) + (chunk >= ap::codex::OtherClassChunk ? " (other classes)" : " (your class)") + " now holds "
				+ std::to_string(count) + " skills.");
			D2RL::Items::ItemInfo info { .structSize = D2RL::Items::ItemInfoSize };
			if (createdCodex != D2RL::InvalidItemHandle && gItems->getItemInfo(gContext, createdCodex, &info) == D2RL::Items::Result::Success) {
				Log("Archipelago: created Codex " + DescribeItem(info) + ".");
			}
		}
	}
	return ok;
}

struct ItemScan {
	std::set<uint8_t>  questFlags;
	// Seeds of the uniques and set items you found (not rewards, not the Codex).
	std::set<uint32_t> uniqueSeeds;
	std::set<uint32_t> setSeeds;
	// Codes and set/unique rows of the items you found yourself (not rewards).
	std::set<uint32_t> codes;
	std::set<int32_t>  setRecords;
	std::set<int32_t>  uniqueRecords;
};

auto __cdecl CollectItems(const D2RL::PluginContext*, const D2RL::Items::ItemInfo* item, void* userData) noexcept -> D2RL::Inventory::IterationAction {
	if (item == nullptr || item->structSize < D2RL::Items::ItemInfoRequiredSize || item->container == D2RL::Items::ItemContainer::Ground
		|| item->container == D2RL::Items::ItemContainer::Trade) {
		return D2RL::Inventory::IterationAction::Continue;
	}
	auto* scan = static_cast<ItemScan*>(userData);
	if (const auto flag = ap::quests::Find(ap::quests::Trigger::Item, item->code)) {
		scan->questFlags.insert(*flag);
	}
	if (!IsReward(*item)) {
		scan->codes.insert(item->code);
		const uint32_t seed = item->itemSeed != 0 ? item->itemSeed : item->generationSeed;
		if (item->quality == D2RL::Items::Quality::Set) {
			scan->setRecords.insert(item->qualityRecordId);
			scan->setSeeds.insert(seed);
		} else if (item->quality == D2RL::Items::Quality::Unique && CodexSeedOf(*item) == 0) {
			scan->uniqueRecords.insert(item->qualityRecordId);
			scan->uniqueSeeds.insert(seed);
		}
	}
	return D2RL::Inventory::IterationAction::Continue;
}

auto AnyRecord(const std::vector<uint32_t>& records, const std::set<int32_t>& found) -> bool {
	return std::ranges::any_of(records, [&](uint32_t record) { return found.contains(static_cast<int32_t>(record)); });
}

// The collection checks the items on the character complete.
auto CollectionChecks(const ItemScan& scan, const ap::SlotData& slots) -> std::vector<int64_t> {
	std::vector<int64_t> checks;
	for (int rune = 1; rune <= std::min(slots.runeChecks, 33); ++rune) {
		char code[5] {};
		std::snprintf(code, sizeof(code), "r%02d", rune);
		if (scan.codes.contains(ap::loot::CodeFromText(code))) {
			checks.push_back(ap::ids::RuneLocation(static_cast<uint32_t>(rune)));
		}
	}
	for (uint32_t index = 0; slots.gemChecks && index < std::size(ap::data::Gems); ++index) {
		if (scan.codes.contains(ap::loot::CodeFromText(ap::data::Gems[index].code))) {
			checks.push_back(ap::ids::GemLocation(index));
		}
	}
	for (uint32_t index = 0; slots.specialItemChecks && index < std::size(ap::data::Specials); ++index) {
		const auto& special = ap::data::Specials[index];
		const bool found = special.unique != nullptr ? AnyRecord(UniqueRecords(special.unique), scan.uniqueRecords)
		                                             : scan.codes.contains(ap::loot::CodeFromText(special.code));
		if (found) {
			checks.push_back(ap::ids::SpecialLocation(index));
		}
	}
	for (uint32_t index = 0; slots.setChecks && index < std::size(ap::data::Sets); ++index) {
		bool complete = true;
		for (const char* piece : ap::data::Sets[index].pieces) {
			if (piece != nullptr && !AnyRecord(SetRecords(piece), scan.setRecords)) {
				complete = false;
				break;
			}
		}
		if (complete) {
			checks.push_back(ap::ids::SetLocation(index));
		}
	}
	return checks;
}

// The check for finding a quest item on a difficulty, if the code is one
// (the Horadric Cube counts once).
auto QuestItemCheck(uint32_t code, int difficulty) -> std::optional<int64_t> {
	for (uint32_t index = 0; index < std::size(ap::data::QuestItems); ++index) {
		const auto& item = ap::data::QuestItems[index];
		if (code == ap::loot::CodeFromText(item.code)) {
			return ap::ids::QuestItemLocation(item.everyDifficulty ? static_cast<uint32_t>(std::clamp(difficulty, 0, 2)) : 0, index);
		}
	}
	return std::nullopt;
}

// Quest items and collections anywhere on the character (worn, inventory,
// belt, cube, stash, cursor).
void ScanItems(D2RL::PlayerHandle player) {
	ItemScan scan;
	const D2RL::Inventory::ItemFilter filter { .structSize = D2RL::Inventory::ItemFilterSize, .containerMask = D2RL::Items::AllItemContainers };
	if (gInventory->forEachInventoryItem(gContext, player, &filter, CollectItems, &scan) != D2RL::Inventory::Result::Success) {
		return;
	}
	const ap::SlotData slots = gClient->Slots();
	auto checks = CollectionChecks(scan, slots);
	std::lock_guard lock(gMutex);
	for (const uint8_t flag : scan.questFlags) {
		if (gDifficultyKnown) {
			gQuestQueue.push_back(static_cast<int>(QuestSource::Item) * 1000 + std::clamp(gDifficulty, 0, 2) * 100 + flag);
		}
	}
	// Quest items, on the difficulty you're on.
	for (const uint32_t code : scan.codes) {
		const auto check = slots.questItemChecks && gDifficultyKnown ? QuestItemCheck(code, gDifficulty) : std::nullopt;
		if (check) {
			checks.push_back(*check);
		}
	}
	// Uniques and set items found, counted across sessions.
	const size_t before = gSave.foundUniques.size() + gSave.foundSets.size();
	gSave.foundUniques.insert(scan.uniqueSeeds.begin(), scan.uniqueSeeds.end());
	gSave.foundSets.insert(scan.setSeeds.begin(), scan.setSeeds.end());
	if (gSave.foundUniques.size() + gSave.foundSets.size() != before) {
		gSave.Save(gSavePath);
	}
	for (uint32_t index = 0; slots.findCountChecks && index < std::size(ap::ids::UniqueFinds); ++index) {
		if (gSave.foundUniques.size() >= ap::ids::UniqueFinds[index]) {
			checks.push_back(ap::ids::UniqueFindLocation(index));
		}
	}
	for (uint32_t index = 0; slots.findCountChecks && index < std::size(ap::ids::SetFinds); ++index) {
		if (gSave.foundSets.size() >= ap::ids::SetFinds[index]) {
			checks.push_back(ap::ids::SetFindLocation(index));
		}
	}
	gCollectionQueue.insert(gCollectionQueue.end(), checks.begin(), checks.end());
}

struct ParcelScan {
	std::vector<std::pair<D2RL::ItemHandle, uint32_t>> parcels;    // handle, act
	std::vector<std::pair<D2RL::ItemHandle, uint32_t>> prophecies; // handle, tier
};

auto __cdecl CollectParcels(const D2RL::PluginContext*, const D2RL::Items::ItemInfo* item, void* userData) noexcept -> D2RL::Inventory::IterationAction {
	if (item != nullptr && item->structSize >= D2RL::Items::ItemInfoRequiredSize) {
		for (uint32_t act = 0; act < std::size(ap::randomizer::ParcelCodes); ++act) {
			if (item->code == ap::loot::CodeFromText(ap::randomizer::ParcelCodes[act])) {
				static_cast<ParcelScan*>(userData)->parcels.emplace_back(item->handle, act);
			}
		}
		for (uint32_t tier = 0; tier < std::size(ap::randomizer::ProphecyCodes); ++tier) {
			if (item->code == ap::loot::CodeFromText(ap::randomizer::ProphecyCodes[tier])) {
				static_cast<ParcelScan*>(userData)->prophecies.emplace_back(item->handle, tier);
			}
		}
	}
	return D2RL::Inventory::IterationAction::Continue;
}

// Shop parcels anywhere on the character: each one bought is that act's next
// shop check, and is removed. Past the last check its price is refunded.
void ScanParcels(D2RL::PlayerHandle player) {
	ParcelScan scan;
	const D2RL::Inventory::ItemFilter filter { .structSize = D2RL::Inventory::ItemFilterSize, .containerMask = D2RL::Items::AllItemContainers };
	if (gInventory->forEachInventoryItem(gContext, player, &filter, CollectParcels, &scan) != D2RL::Inventory::Result::Success) {
		return;
	}
	const uint32_t checks = static_cast<uint32_t>(std::clamp(gClient->Slots().shopChecks, 0, static_cast<int>(ap::ids::MaxShopChecks)));
	for (const auto& [parcel, act] : scan.parcels) {
		const auto result = gItems->destroyItem(gContext, player, parcel, D2RL::Items::SocketedItemPolicy::DestroyContents);
		if (result != D2RL::Items::Result::Success) {
			Log("Archipelago: couldn't remove a shop parcel (" + std::to_string(static_cast<uint32_t>(result)) + ").");
			continue;
		}
		std::lock_guard lock(gMutex);
		auto taken = [&](int64_t location) {
			return gSave.checks.contains(location) || std::ranges::find(gCollectionQueue, location) != gCollectionQueue.end();
		};
		uint32_t number = 0;
		while (number < checks && taken(ap::ids::ShopLocation(act, number))) {
			++number;
		}
		if (number < checks) {
			gCollectionQueue.push_back(ap::ids::ShopLocation(act, number));
			gConsoleQueue.push_back("Archipelago: parcel delivered (" + std::string(ActNames[act]) + " shop, " + std::to_string(number + 1) + " of "
			                        + std::to_string(checks) + ").");
		} else {
			gCommandQueue.push_back("gold " + std::to_string(ap::randomizer::ParcelCosts[act]));
			gConsoleQueue.push_back("Archipelago: no more parcels to deliver in " + std::string(ActNames[act]) + "; your gold is refunded.");
		}
	}
	// Scrolls of Prophecy: only the next price tier counts (the last one repeats);
	// a cheaper one is refunded.
	const bool prophecyShop = gClient->Slots().prophecyShop;
	constexpr int LastTier = static_cast<int>(std::size(ap::randomizer::ProphecyCodes)) - 1;
	for (const auto& [scroll, tier] : scan.prophecies) {
		if (gItems->destroyItem(gContext, player, scroll, D2RL::Items::SocketedItemPolicy::DestroyContents) != D2RL::Items::Result::Success) {
			continue;
		}
		std::lock_guard lock(gMutex);
		const int next = std::min(gSave.prophecies, LastTier);
		if (!prophecyShop || static_cast<int>(tier) < next) {
			gCommandQueue.push_back("gold " + std::to_string(ap::randomizer::ProphecyCosts[tier]));
			gConsoleQueue.push_back(!prophecyShop ? "Archipelago: Scrolls of Prophecy aren't part of this seed; your gold is refunded."
			                                      : "Archipelago: you already know that prophecy; the next is the " + std::to_string(ap::randomizer::ProphecyCosts[next])
			                                            + "-gold scroll. Your gold is refunded.");
			continue;
		}
		gSave.prophecies = static_cast<int>(tier) + 1;
		gSave.Save(gSavePath);
		gHintQueue.push_back("The Scroll of Prophecy reads:");
	}
}

// ---- Town Portal lock (game thread) ---------------------------------------------
//
// D2RLoader doesn't report Town Portal scroll use, so it can't be blocked like a
// potion. Instead, while Town Portal is locked, scrolls and tomes on the
// character are taken away; Town Portal Access then brings a Tome of Town Portal.

auto __cdecl CollectPortals(const D2RL::PluginContext*, const D2RL::Items::ItemInfo* item, void* userData) noexcept -> D2RL::Inventory::IterationAction {
	static const uint32_t scroll = ap::loot::CodeFromText("tsc"), tome = ap::loot::CodeFromText("tbk");
	if (item != nullptr && item->structSize >= D2RL::Items::ItemInfoRequiredSize && (item->code == scroll || item->code == tome)
		&& item->container != D2RL::Items::ItemContainer::Ground && item->container != D2RL::Items::ItemContainer::Trade) {
		static_cast<std::vector<D2RL::ItemHandle>*>(userData)->push_back(item->handle);
	}
	return D2RL::Inventory::IterationAction::Continue;
}

void EnforceTownPortalLock(D2RL::PlayerHandle player) {
	bool locked = false, lockInSeed = false, given = false;
	{
		std::lock_guard lock(gMutex);
		locked = gLocks.townPortal;
		lockInSeed = gSlots.locks.townPortal;
		given = gSave.portalTomeGiven;
	}
	if (!locked) {
		if (!lockInSeed || given) {
			return;
		}
		auto spec = MakeSpec(ap::loot::CodeFromText("tbk"), D2RL::Items::Quality::Normal, D2RL::Items::ItemContainer::Inventory);
		spec.quantity = 20;
		D2RL::ItemHandle item = D2RL::InvalidItemHandle;
		auto result = gItems->createItem(gContext, player, &spec, &item);
		if (result != D2RL::Items::Result::Success) {
			spec.destination.container = D2RL::Items::ItemContainer::Ground;
			result = gItems->createItem(gContext, player, &spec, &item);
		}
		if (result != D2RL::Items::Result::Success) {
			Log("Archipelago: couldn't give the Tome of Town Portal (" + std::to_string(static_cast<uint32_t>(result)) + ").");
			return;
		}
		{
			std::lock_guard lock(gMutex);
			gSave.portalTomeGiven = true;
			gSave.Save(gSavePath);
		}
		Feed("Archipelago: Town Portal Access! Here's a Tome of Town Portal.");
		return;
	}
	std::vector<D2RL::ItemHandle> portals;
	const D2RL::Inventory::ItemFilter filter { .structSize = D2RL::Inventory::ItemFilterSize, .containerMask = D2RL::Items::AllItemContainers };
	if (gInventory->forEachInventoryItem(gContext, player, &filter, CollectPortals, &portals) != D2RL::Inventory::Result::Success) {
		return;
	}
	int removed = 0;
	for (const auto item : portals) {
		if (gItems->destroyItem(gContext, player, item, D2RL::Items::SocketedItemPolicy::DestroyContents) == D2RL::Items::Result::Success) {
			++removed;
		}
	}
	if (removed > 0) {
		Log("Archipelago: removed " + std::to_string(removed) + " Town Portal scrolls or tomes.");
		Feed("Archipelago: Town Portal is locked until you find Town Portal Access, so your Town Portal scrolls crumble.");
	}
}

// ---- Potion lock (game thread) ---------------------------------------------------
//
// D2RLoader reports drinking from the belt, which OnItemActivate blocks, but not
// right-clicking a potion in the inventory. So while potions are locked, potions
// in the inventory are set aside, and Potion Access hands them back.

auto IsPotion(uint32_t code) -> bool {
	static const std::set<uint32_t> potions = [] {
		std::set<uint32_t> codes;
		for (const char* text : { "hp1", "hp2", "hp3", "hp4", "hp5", "mp1", "mp2", "mp3", "mp4", "mp5", "rvs", "rvl", "yps", "vps", "wms" }) {
			codes.insert(ap::loot::CodeFromText(text));
		}
		return codes;
	}();
	return potions.contains(code);
}

auto __cdecl CollectPotions(const D2RL::PluginContext*, const D2RL::Items::ItemInfo* item, void* userData) noexcept -> D2RL::Inventory::IterationAction {
	if (item != nullptr && item->structSize >= D2RL::Items::ItemInfoRequiredSize && item->container == D2RL::Items::ItemContainer::Inventory
		&& IsPotion(item->code)) {
		static_cast<std::vector<std::pair<D2RL::ItemHandle, uint32_t>>*>(userData)->emplace_back(item->handle, item->code);
	}
	return D2RL::Inventory::IterationAction::Continue;
}

void EnforcePotionLock(D2RL::PlayerHandle player) {
	bool locked = false;
	std::vector<uint32_t> held;
	{
		std::lock_guard lock(gMutex);
		locked = gLocks.potions;
		held = gSave.heldPotions;
	}
	if (!locked) {
		if (held.empty()) {
			return;
		}
		std::vector<uint32_t> kept;
		for (const uint32_t code : held) {
			auto spec = MakeSpec(code, D2RL::Items::Quality::Normal, D2RL::Items::ItemContainer::Inventory);
			D2RL::ItemHandle item = D2RL::InvalidItemHandle;
			auto result = gItems->createItem(gContext, player, &spec, &item);
			if (result != D2RL::Items::Result::Success) {
				spec.destination.container = D2RL::Items::ItemContainer::Ground;
				result = gItems->createItem(gContext, player, &spec, &item);
			}
			if (result != D2RL::Items::Result::Success) {
				kept.push_back(code);
			}
		}
		{
			std::lock_guard lock(gMutex);
			gSave.heldPotions = kept;
			gSave.Save(gSavePath);
		}
		if (kept.size() < held.size()) {
			Feed("Archipelago: Potion Access! Your " + std::to_string(held.size() - kept.size()) + " set-aside potions are back.");
		}
		if (!kept.empty()) {
			Log("Archipelago: couldn't give back " + std::to_string(kept.size()) + " potions; trying again later.");
		}
		return;
	}
	std::vector<std::pair<D2RL::ItemHandle, uint32_t>> potions;
	const D2RL::Inventory::ItemFilter filter { .structSize = D2RL::Inventory::ItemFilterSize,
		.containerMask = D2RL::Items::ContainerBit(D2RL::Items::ItemContainer::Inventory) };
	if (gInventory->forEachInventoryItem(gContext, player, &filter, CollectPotions, &potions) != D2RL::Inventory::Result::Success) {
		return;
	}
	std::vector<uint32_t> removed;
	for (const auto& [item, code] : potions) {
		if (gItems->destroyItem(gContext, player, item, D2RL::Items::SocketedItemPolicy::DestroyContents) == D2RL::Items::Result::Success) {
			removed.push_back(code);
		}
	}
	if (removed.empty()) {
		return;
	}
	{
		std::lock_guard lock(gMutex);
		gSave.heldPotions.insert(gSave.heldPotions.end(), removed.begin(), removed.end());
		gSave.Save(gSavePath);
	}
	Log("Archipelago: set aside " + std::to_string(removed.size()) + " potions from the inventory.");
	Feed("Archipelago: potions are locked until you find Potion Access, so " + std::to_string(removed.size())
		 + " potions from your inventory are set aside until then.");
}

// The superunique copies are the last MonStats rows of each bank.
void FindSuperUniqueMonsters() {
	int bases[2] = { -1, -1 };
	int slot = 0;
	for (const auto bank : { D2RL::DataTables::Bank::Rotw, D2RL::DataTables::Bank::Lod }) {
		D2RL::DataTables::TableView view { .structSize = D2RL::DataTables::TableViewSize };
		if (gTables != nullptr && gTables->getTable(gContext, bank, D2RL::DataTables::TableId::MonStats, &view) == D2RL::DataTables::Result::Success
			&& view.rowCount >= std::size(ap::data::SuperUniques)) {
			bases[slot] = static_cast<int>(view.rowCount - std::size(ap::data::SuperUniques));
		}
		++slot;
	}
	std::lock_guard lock(gMutex);
	if (bases[0] != gSuperUniqueBase[0] || bases[1] != gSuperUniqueBase[1]) {
		gSuperUniqueBase[0] = bases[0];
		gSuperUniqueBase[1] = bases[1];
		Log("Archipelago: superunique monsters start at row " + std::to_string(bases[0]) + " (RotW) and " + std::to_string(bases[1]) + " (LoD).");
	}
}

// Hands the character its Archipelago Ledger once per seed (ap-ledger asks for
// another). If it can't be made (no room, or a mod without it), retries later.
Clock::time_point gLedgerRetryAt;

auto __cdecl FindLedger(const D2RL::PluginContext*, const D2RL::Items::ItemInfo* item, void* userData) noexcept -> D2RL::Inventory::IterationAction {
	if (item != nullptr && item->structSize >= D2RL::Items::ItemInfoRequiredSize && item->code == ap::loot::CodeFromText(ap::randomizer::LedgerCode)) {
		*static_cast<bool*>(userData) = true;
		return D2RL::Inventory::IterationAction::Stop;
	}
	return D2RL::Inventory::IterationAction::Continue;
}

void GiveLedger(D2RL::PlayerHandle player) {
	{
		std::lock_guard lock(gMutex);
		if (gSave.ledgerGiven || Clock::now() < gLedgerRetryAt) {
			return;
		}
	}
	bool found = false;
	const D2RL::Inventory::ItemFilter filter { .structSize = D2RL::Inventory::ItemFilterSize, .containerMask = D2RL::Items::AllItemContainers };
	if (gInventory->forEachInventoryItem(gContext, player, &filter, FindLedger, &found) != D2RL::Inventory::Result::Success) {
		return;
	}
	if (!found) {
		auto spec = MakeSpec(ap::loot::CodeFromText(ap::randomizer::LedgerCode), D2RL::Items::Quality::Normal, D2RL::Items::ItemContainer::Inventory);
		D2RL::ItemHandle item = D2RL::InvalidItemHandle;
		const auto result = gItems->createItem(gContext, player, &spec, &item);
		if (result != D2RL::Items::Result::Success) {
			Log("Archipelago: couldn't make the Archipelago Ledger (result " + std::to_string(static_cast<uint32_t>(result))
				+ "); make room in your inventory, or rerun d2r-ap-setup if the mod predates it.");
			std::lock_guard lock(gMutex);
			gLedgerRetryAt = Clock::now() + std::chrono::seconds(60);
			return;
		}
		Feed("Archipelago: your Archipelago Ledger counts the checks done in each act; hover over it.");
	}
	std::lock_guard lock(gMutex);
	gSave.ledgerGiven = true;
	gSave.Save(gSavePath);
}

// Tree mode: unlocks each received skill in the tree with D2RCore's skadd (its
// first point; the tree's self-prerequisite lock then lets you spend more), and
// hands out extra copies and Skill Points as unspent skill points.
void DeliverTreeSkills(const std::vector<ap::NetworkItem>& items) {
	std::vector<std::string> news;
	{
		std::lock_guard lock(gMutex);
		const auto plan = ap::codex::PlanTreeDelivery(items, gSave.unlockedSkills, gSave.skillPointsGiven);
		if (plan.unlock.empty() && plan.points == 0) {
			return;
		}
		for (const uint32_t skill : plan.unlock) {
			gCommandQueue.push_back("skadd " + SkillName(skill) + ",1");
			gSave.unlockedSkills.insert(skill);
			news.push_back("Archipelago: " + SkillName(skill) + " is unlocked in your skill tree.");
		}
		if (plan.points > 0) {
			gCommandQueue.push_back("skillpoints " + std::to_string(plan.points));
			gSave.skillPointsGiven += plan.points;
			news.push_back("Archipelago: +" + std::to_string(plan.points) + (plan.points == 1 ? " skill point." : " skill points."));
		}
		gSave.Save(gSavePath);
	}
	for (const auto& line : news) {
		Feed(line);
	}
}

// Tree mode: the unlocked skills that won't take a point, or nothing if a
// check failed. A respec takes away the level skadd gave, but canAllocateSkill
// also says no with no unspent points, below a skill's level requirement or at
// its cap, so on its own this doesn't prove a respec.
auto RefusedSkills(D2RL::PlayerHandle player) -> std::optional<std::vector<uint32_t>> {
	std::set<uint32_t> unlocked;
	{
		std::lock_guard lock(gMutex);
		unlocked = gSave.unlockedSkills;
	}
	std::vector<uint32_t> refused;
	for (const uint32_t skill : unlocked) {
		bool allowed = false;
		if (gGameRules->canAllocateSkill(gContext, player, skill, &allowed) != D2RL::GameRules::Result::Success) {
			return std::nullopt;
		}
		if (!allowed) {
			refused.push_back(skill);
		}
	}
	return refused;
}

auto SkillNames(const std::vector<uint32_t>& skills) -> std::string {
	std::string names;
	for (const uint32_t skill : skills) {
		names += (names.empty() ? "" : ", ") + SkillName(skill);
	}
	return names;
}

// Tree mode: after a Token or Writ of Absolution, unlocks the received skills again.
// The token counts once it's gone (used, not just clicked); then a respec has
// taken every received skill's level, so each one refuses a point. If any
// still takes one, the token didn't respec and nothing changes.
void RelearnAfterToken(D2RL::PlayerHandle player) {
	D2RL::ItemHandle  token = D2RL::InvalidItemHandle;
	Clock::time_point usedAt;
	{
		std::lock_guard lock(gMutex);
		token = gRespecToken;
		usedAt = gRespecTokenAt;
	}
	if (token == D2RL::InvalidItemHandle || gGameRules == nullptr) {
		return;
	}
	D2RL::Items::ItemInfo info { .structSize = D2RL::Items::ItemInfoSize };
	const auto result = gItems->getItemInfo(gContext, token, &info);
	if (result != D2RL::Items::Result::NotFound && result != D2RL::Items::Result::StaleHandle) {
		if (Clock::now() - usedAt > std::chrono::seconds(30)) {
			Log("Archipelago: the token is still there, so it didn't respec.");
			std::lock_guard lock(gMutex);
			gRespecToken = D2RL::InvalidItemHandle;
		}
		return;
	}
	{
		std::lock_guard lock(gMutex);
		gRespecToken = D2RL::InvalidItemHandle;
	}
	const auto refused = RefusedSkills(player);
	size_t unlocked = 0;
	{
		std::lock_guard lock(gMutex);
		unlocked = gSave.unlockedSkills.size();
	}
	if (!refused) {
		Feed("Archipelago: couldn't check your skills after the respec; use Relearn Skills (F9) to unlock them again.");
		return;
	}
	if (refused->empty()) {
		return;
	}
	if (refused->size() != unlocked) {
		Log("Archipelago: some Archipelago skills still take a point after the token, so they weren't unlocked again.");
		return;
	}
	{
		std::lock_guard lock(gMutex);
		for (const uint32_t skill : *refused) {
			gCommandQueue.push_back("skadd " + SkillName(skill) + ",1");
		}
	}
	Feed("Archipelago: your respec took your Archipelago skills, so they're unlocked again: " + SkillNames(*refused) + ".");
}

void __cdecl GameWork(const D2RL::PluginContext*, void*) noexcept {
	gWorkQueued = false;
	D2RL::PlayerHandle player = D2RL::InvalidPlayerHandle;
	if (gInventory->getLocalPlayer(gContext, &player) != D2RL::Inventory::Result::Success) {
		return;
	}
	FindSuperUniqueMonsters();
	ReadAmbushArea();
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

	ScanItems(player);
	RelearnAfterToken(player);
	ScanParcels(player);
	EnforceTownPortalLock(player);
	EnforcePotionLock(player);
	GiveLedger(player);
	{
		auto records = UniqueRecords("Archipelago Codex");
		std::lock_guard lock(gMutex);
		gCodexRecords = std::move(records);
	}
	{
		// After a failed item update, only scan until the retry pause is over.
		std::lock_guard lock(gMutex);
		if (Clock::now() < gRetryAt) {
			return;
		}
	}
	const ap::SlotData slots = gClient->Slots();
	auto skills = ap::codex::Entries(items, EffectiveLevel(), slots.skillScaling, slots.skillScalingCap);
	{
		// Lord of Destruction characters have no Warlock skills (D2R asserts on them).
		std::vector<std::string> dropped;
		{
			std::lock_guard lock(gMutex);
			if (gLodCharacter) {
				std::erase_if(skills, [&](const ap::codex::Entry& entry) {
					if (!IsWarlockSkill(entry.skill)) {
						return false;
					}
					if (gWarnedWarlockSkills.insert(entry.skill).second) {
						dropped.push_back(SkillName(entry.skill));
					}
					return true;
				});
			}
		}
		for (const auto& name : dropped) {
			Feed("Archipelago: " + name + " (Warlock) needs a Reign of the Warlock character; it's left off your Codex.");
		}
	}
	int playerClass = -1;
	{
		std::lock_guard lock(gMutex);
		playerClass = gPlayerClass;
	}
	if (playerClass < 0) {
		playerClass = ClassIndex(gClient->Slots().characterClass);
	}
	// Tree mode unlocks skills in the tree instead of building a Codex.
	const bool treeMode = gClient->Slots().randomizer.treeMode;
	if (treeMode) {
		DeliverTreeSkills(items);
	}
	const bool codexOk = treeMode || SyncCodex(player, skills, playerClass, config);

	std::mt19937 random(static_cast<uint32_t>(Clock::now().time_since_epoch().count()));
	const int characterLevel = EffectiveLevel();
	int act = 0;
	{
		std::lock_guard lock(gMutex);
		act = gAct;
	}
	size_t next = std::min(delivered, items.size());
	bool   roomFull = false;
	bool   more     = false;
	size_t given    = 0;
	for (; next < items.size(); ++next) {
		if (!ap::ids::IsLootItem(items[next].item)) {
			continue;
		}
		if (given >= LootPerPass) {
			more = true;
			break;
		}
		const auto loot = static_cast<ap::loot::Loot>(items[next].item - ap::ids::LootBase);
		// A Junk Backpack spawns a pile of items, so it fills a whole pass.
		given += loot == ap::loot::Loot::JunkBackpack ? LootPerPass : 1;
		if (loot == ap::loot::Loot::JunkBackpack) {
			Feed("Archipelago: a Junk Backpack! " + std::to_string(GiveJunk(player, std::clamp(slots.traps.junkSize, 1, 40), random))
				 + " pieces of junk in your inventory.");
			continue;
		}
		if (loot == ap::loot::Loot::Prophecy) {
			std::lock_guard lock(gMutex);
			gHintQueue.push_back("A prophecy arrives from the multiworld:");
			continue;
		}
		if (auto commands = ap::loot::CommandsFor(loot, characterLevel, act, random); !commands.commands.empty()) {
			std::lock_guard lock(gMutex);
			if (commands.trap) {
				gTrapQueue.push_back({ loot, characterLevel });
			} else {
				gCommandQueue.insert(gCommandQueue.end(), commands.commands.begin(), commands.commands.end());
				gConsoleQueue.push_back("Archipelago: " + commands.message);
			}
			continue;
		}
		if (loot == ap::loot::Loot::Gold || loot == ap::loot::Loot::Experience) {
			const bool gold = loot == ap::loot::Loot::Gold;
			const int64_t amount = gold ? ap::loot::GoldAmount(characterLevel) : ap::loot::ExperienceAmount(characterLevel);
			if (amount > 0) {
				std::lock_guard lock(gMutex);
				gCommandQueue.push_back((gold ? "gold " : "experience ") + std::to_string(amount));
			}
			Feed("Archipelago: you got " + std::to_string(amount) + (gold ? " gold." : " experience."));
			continue;
		}
		// An item D2R refuses everywhere may not exist for this character, so roll
		// others. If none fit, a mid rune stands in; if even that won't fit, the
		// containers are full and we wait for the player to make room.
		// Uniques and sets from another player carry that player's name.
		const int         sender = items[next].player;
		const std::string owner = sender > 0 && sender != gClient->MySlot() ? ap::personal::Name(gClient->PlayerName(sender)) : std::string {};
		ap::loot::Pick pick {};
		bool placed = false;
		for (int attempt = 0; attempt < 5 && !placed; ++attempt) {
			pick = ap::loot::Roll(loot, characterLevel, random, playerClass);
			if (pick.code == 0) {
				break;
			}
			placed = GiveLoot(player, pick, owner);
			if (!placed) {
				Log("Archipelago: couldn't place " + pick.name + "; rolling another.");
			}
		}
		if (pick.code == 0) {
			continue;
		}
		if (!placed) {
			pick = ap::loot::Roll(ap::loot::Loot::MidRune, characterLevel, random);
			placed = pick.code != 0 && GiveLoot(player, pick);
			if (!placed) {
				roomFull = true;
				break;
			}
			Log("Archipelago: gave " + pick.name + " in place of a reward that wouldn't fit.");
		}
		if (pick.quality == D2RL::Items::Quality::Unique || pick.quality == D2RL::Items::Quality::Set || loot == ap::loot::Loot::HighRune) {
			Feed("Archipelago: you got " + pick.name + (owner.empty() ? std::string {} : " from " + owner) + "!");
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
	} else if (codexOk && more) {
		gRetryAt = Clock::now() + LootPassGap;
	} else {
		gRetryAt = Clock::now() + std::chrono::seconds(30);
	}
	if (!codexOk && Clock::now() - gLastRoomWarning > std::chrono::seconds(30)) {
		gLastRoomWarning = Clock::now();
		gConsoleQueue.push_back("Archipelago: couldn't place the Codex charm; make room in your inventory (details in the log).");
	}
	if (roomFull && Clock::now() - gLastRoomWarning > std::chrono::seconds(30)) {
		gLastRoomWarning = Clock::now();
		// Only the containers rewards can go to: a locked cube or stash is skipped.
		const std::string rooms = gLocks.cube && gLocks.stash ? "inventory"
		                          : gLocks.cube               ? "inventory or stash"
		                          : gLocks.stash              ? "inventory or cube"
		                                                      : "inventory, cube or stash";
		gConsoleQueue.push_back("Archipelago: rewards are waiting; make room in your " + rooms + ".");
	}
}

// ---- DeathLink (UI thread) --------------------------------------------------

// D2RCore's own top-level commands; everything else it runs (gold, experience,
// mspawn, killme, skadd...) is a cheat, typed as "cheat <name> <args>".
constexpr std::string_view BuiltinCommands[] = { "class", "clearcorpses", "corpseclear", "goto", "loc", "spawnloc", "clearitems", "cubeinputs", "god", "heal" };

// Adds "cheat " in front of a command unless it's one of D2RCore's top-level ones.
auto ConsoleCommandText(const std::string& command) -> std::string {
	const std::string_view name = std::string_view(command).substr(0, command.find(' '));
	return std::ranges::find(BuiltinCommands, name) != std::end(BuiltinCommands) || name == "cheat" ? command : "cheat " + command;
}

// Runs a D2RCore console command, cheat commands included (ExecuteConsoleCommand
// turns the cheat flag on for the one call). UI thread only. True if D2RCore
// accepted it, which doesn't mean the command exists or worked.
auto ExecuteCommand(const std::string& command) -> bool {
	using ExecuteFn = D2RL::CoreExports::ExecuteConsoleCommandFn;
	static const auto execute = [] {
		const HMODULE core = GetModuleHandleW(L"D2RCore.dll");
		return core != nullptr ? reinterpret_cast<ExecuteFn>(GetProcAddress(core, MAKEINTRESOURCEA(D2RL::CoreExports::ExecuteConsoleCommandInfo.ordinal)))
		                       : nullptr;
	}();
	const std::string text = ConsoleCommandText(command);
	const bool accepted = execute != nullptr && execute(text.c_str());
	Log("Archipelago: console command \"" + text + "\" " + (accepted ? "accepted." : "refused."));
	return accepted;
}

// Gold and experience rewards: D2RCore's gold and experience commands add to
// the player's stats.
void __cdecl RunCommands(const D2RL::PluginContext*, void*) noexcept {
	gCommandsQueued = false;
	const ap::SlotData::Traps settings = gClient->Slots().traps;
	std::vector<std::string> commands;
	std::vector<QueuedTrap> traps;
	int area = -1, act = 0, difficulty = 0;
	{
		std::lock_guard lock(gMutex);
		if (!gInGame) {
			return;
		}
		if (Clock::now() >= gNextCommandsAt && !gCommandQueue.empty()) {
			const auto take = static_cast<std::ptrdiff_t>(std::min(gCommandQueue.size(), CommandsPerRun));
			commands.assign(gCommandQueue.begin(), gCommandQueue.begin() + take);
			gCommandQueue.erase(gCommandQueue.begin(), gCommandQueue.begin() + take);
			gNextCommandsAt = Clock::now() + CommandGap;
		}
		if (!gTrapQueue.empty() && Clock::now() >= gNextTrapAt && (!settings.waitForTown || !IsTown(gCurrentLevel))) {
			traps.push_back(gTrapQueue.front());
			gTrapQueue.erase(gTrapQueue.begin());
		}
		area = gCurrentLevel;
		act = gAct;
		difficulty = gDifficulty;
	}
	for (const auto& command : commands) {
		if (!ExecuteCommand(command)) {
			Feed("Archipelago: couldn't add a reward (D2RCore refused \"" + command + "\").");
		}
	}
	if (traps.empty()) {
		return;
	}
	// An area ambush waits (a few seconds at most) for the game thread to read
	// this area's monsters; after that it uses the act's.
	std::vector<std::string> monsters;
	if (settings.areaAmbush) {
		std::lock_guard lock(gMutex);
		if (gAmbushArea.level == area && gAmbushArea.difficulty == difficulty) {
			monsters = gAmbushArea.monsters;
		} else {
			const auto now = Clock::now();
			std::erase_if(traps, [&](QueuedTrap& trap) {
				if (trap.loot != ap::loot::Loot::AmbushTrap) {
					return false;
				}
				if (trap.waiting == Clock::time_point {}) {
					trap.waiting = now;
				}
				if (now - trap.waiting > std::chrono::seconds(10)) {
					return false;
				}
				gTrapQueue.insert(gTrapQueue.begin(), trap);
				return true;
			});
			gNextQuestScan = now;
		}
	}
	std::mt19937 random(static_cast<uint32_t>(Clock::now().time_since_epoch().count()));
	const ap::loot::TrapSettings tuning { .ambushSize = settings.ambushSize, .taxPerLevel = settings.taxPerLevel };
	if (!traps.empty()) {
		std::lock_guard lock(gMutex);
		gNextTrapAt = Clock::now() + TrapGap;
	}
	for (const auto& trap : traps) {
		const auto fired = ap::loot::CommandsFor(trap.loot, trap.level, act, random, tuning, monsters);
		Feed("Archipelago: " + fired.message);
		for (const auto& command : fired.commands) {
			ExecuteCommand(command);
		}
	}
}

// D2RCore's killme console command kills the player through the game's own
// death code.
void __cdecl KillPlayer(const D2RL::PluginContext*, void*) noexcept {
	gKillQueued = false;
	{
		std::lock_guard lock(gMutex);
		const bool kill = gDeathPending && gInGame && !gPlayerDead;
		gDeathPending = false;
		if (!kill) {
			return;
		}
		gLinkKillAt = Clock::now();
	}
	if (!ExecuteCommand("killme")) {
		Feed("Archipelago: DeathLink couldn't kill your character (D2RCore refused killme).");
	}
}

// The player died (or came back without us seeing the death): tell the other
// DeathLink players, unless DeathLink caused it.
void PlayerDied(const std::string& how) {
	bool echo = false;
	{
		std::lock_guard lock(gMutex);
		if (gPlayerDead || !gInGame) {
			return;
		}
		gPlayerDead = true;
		gRepairOffer.Cancel();
		gDeathPending = false;
		echo = Clock::now() - gLinkKillAt < std::chrono::seconds(20);
	}
	Log("Archipelago: you died (" + how + ").");
	if (echo || !gClient || !gClient->Slots().deathLink) {
		return;
	}
	gClient->SendDeath(gClient->Slot() + " died in Diablo II: Resurrected.");
	Feed("Archipelago: you died; DeathLink sent.");
}

// ---- Locks -------------------------------------------------------------------

// Recomputes what's locked when the received items or the seed change.
void UpdateLocks() {
	static uint64_t seen = UINT64_MAX;
	const uint64_t generation = gClient->ItemsGeneration() ^ (gClient->SessionGeneration() << 40);
	if (generation == seen) {
		return;
	}
	seen = generation;
	const ap::SlotData slots = gClient->Slots();
	std::set<int64_t> have;
	for (const auto& item : gClient->Items()) {
		if (ap::ids::IsLockItem(item.item)) {
			have.insert(item.item);
		}
	}
	auto locked = [&](bool on, uint32_t lock) { return on && !have.contains(ap::ids::LockBase + lock); };
	using Lock = ap::ids::Lock;
	LockState locks {
		.cube       = locked(slots.locks.cube, static_cast<uint32_t>(Lock::Cube)),
		.townPortal = locked(slots.locks.townPortal, static_cast<uint32_t>(Lock::TownPortal)),
		.potions    = locked(slots.locks.potions, static_cast<uint32_t>(Lock::Potions)),
		.stash      = locked(slots.locks.stash, static_cast<uint32_t>(Lock::Stash)),
		.merchants  = locked(slots.locks.merchants, static_cast<uint32_t>(Lock::Merchants)),
	};
	// Only Acts I to III: from Acts IV and V a waypoint is the only way back an act, so
	// locking them could strand a check (older seeds still hold their Waypoints items).
	for (uint32_t act = 0; act < 3; ++act) {
		locks.waypoints[act] = locked(slots.locks.waypoints, static_cast<uint32_t>(Lock::Waypoints) + act);
	}
	Log(std::string("Archipelago: locked: cube ") + (locks.cube ? "yes" : "no") + ", town portal " + (locks.townPortal ? "yes" : "no") + ", potions "
	    + (locks.potions ? "yes" : "no") + ", stash " + (locks.stash ? "yes" : "no") + ", merchants " + (locks.merchants ? "yes" : "no")
	    + ", waypoints " + (locks.waypoints[0] ? "I " : "") + (locks.waypoints[1] ? "II " : "") + (locks.waypoints[2] ? "III" : "") + ".");
	std::lock_guard lock(gMutex);
	if (gLocks.merchants != locks.merchants) {
		gRepairOffer.Cancel();
	}
	gLocks = locks;
	gSlots = slots;
}

// Tells the player why something didn't work, at most every few seconds.
void LockMessage(const std::string& text) {
	Log("Archipelago: blocked: " + text);
	{
		std::lock_guard lock(gMutex);
		if (Clock::now() - gLastLockMessage < std::chrono::seconds(3)) {
			return;
		}
		gLastLockMessage = Clock::now();
	}
	Feed("Archipelago: " + text);
}

// Why a D2R panel may not open right now, or empty if it may.
auto LockedPanel(std::string panel) -> std::string {
	std::erase(panel, '"');
	// D2R's names vary with the layout: BankExpansionLayout, VendorPanelLayout,
	// WaypointsPanelExpansion, and Original/Classic variants.
	const auto is = [&](std::string_view prefix) { return panel.starts_with(prefix); };
	std::lock_guard lock(gMutex);
	const int act = std::clamp(gAct, 0, 4);
	if (is("Waypoints") && gLocks.waypoints[act]) {
		return std::string("the waypoints are locked until you find ") + ActNames[act] + " Waypoints.";
	}
	if (is("Bank") && gLocks.stash) {
		return "your stash is locked until you find Stash Access.";
	}
	if ((is("HoradricCube") || is("Cube")) && gLocks.cube) {
		return "the Horadric Cube is locked until you find Horadric Cube Access.";
	}
	if (is("Vendor") && gLocks.merchants) {
		return "merchants won't trade with you until you find Merchant Access.";
	}
	return {};
}

// The stock Vendor panel is never allowed through Merchant Lock. The prototype
// only broadcasts the native RepairAll action from this separate modal, using
// the NPC interaction D2R established before asking to open its vendor panel.
auto RepairEligible() -> bool {
	// gMutex is held by the caller.
	return gConfig.repairOnly && gInGame && !gPlayerDead && IsTown(gCurrentLevel)
		&& gBoundSession != 0 && gClient != nullptr && gClient->State() == ap::ConnectionState::Connected
		&& gClient->SessionGeneration() == gBoundSession && gLocks.merchants && gWidgets != nullptr && gPanels != nullptr
		&& gRepairPanel != D2RL::Panels::InvalidHandle;
}

void __cdecl OpenRepairWork(const D2RL::PluginContext*, void*) noexcept {
	std::string vendor;
	{
		std::lock_guard lock(gMutex);
		gRepairOfferQueued = false;
		if (!gRepairOffer.Ready(RepairEligible(), Clock::now())) {
			gRepairOffer.Cancel();
			return;
		}
		vendor = gRepairVendorPanel;
	}
	D2RL::Widgets::WidgetHandle widget = D2RL::Widgets::InvalidHandle;
	if (gWidgets->findPanel(gContext, vendor.c_str(), &widget) != D2RL::Widgets::Result::Success
		|| gPanels->openPanel(gContext, gRepairPanel) != D2RL::Panels::Result::Success) {
		{
			std::lock_guard lock(gMutex);
			gRepairOffer.Cancel();
		}
		Feed("Archipelago: the repair prototype couldn't find the NPC panel. Shopping remains locked.");
		Log("Archipelago: repair offer failed for " + vendor + ".");
	}
}

bool OfferRepairs(std::string_view vendor) {
	{
		std::lock_guard lock(gMutex);
		if (!RepairEligible()) {
			return false;
		}
		gRepairVendorPanel = ap::repairs::PanelName(vendor);
		gRepairOffer.Begin(Clock::now());
		if (gRepairOfferQueued) {
			return true;
		}
		gRepairOfferQueued = true;
	}
	if (gThreads->runOnUiThread(gContext, OpenRepairWork, nullptr) != D2RL::Threads::Result::Success) {
		std::lock_guard lock(gMutex);
		gRepairOfferQueued = false;
		gRepairOffer.Cancel();
		return false;
	}
	return true;
}

auto MouseButtonDown(int button) -> bool {
	return (GetKeyState(button) & 0x8000) != 0;
}

// D2RLoader also reports Shift- and Ctrl-left-clicks, which move an item (to the
// belt, stash or Cube) rather than use it. The event doesn't say which button was
// pressed, so only a held left button counts; Shift- or Ctrl-right-click still
// uses the item. Shift on a belt potion feeds the mercenary, which is a use too.
auto IsItemMove(const D2RL::ItemInteractions::ItemInteractionEvent& event) -> bool {
	using D2RL::ItemInteractions::Modifier;
	const uint32_t move = D2RL::ItemInteractions::ModifierBit(Modifier::Control)
		| (event.container == D2RL::Items::ItemContainer::Belt ? 0U : D2RL::ItemInteractions::ModifierBit(Modifier::Shift));
	return event.inputSource == D2RL::ItemInteractions::InputSource::KeyboardMouse && (event.modifiers & move) != 0
		&& MouseButtonDown(VK_LBUTTON) && !MouseButtonDown(VK_RBUTTON);
}

// Blocks reading Town Portal scrolls and tomes, and drinking potions, while locked.
// Also records quest items used at once (the Book of Skill, Horadric Scroll, Potion
// of Life and Scroll of Resistance), which are gone before the next item scan.
auto __cdecl OnItemActivate(const D2RL::PluginContext*, const D2RL::ItemInteractions::ItemInteractionEvent* event, void*) noexcept
	-> D2RL::ItemInteractions::Decision {
	using Decision = D2RL::ItemInteractions::Decision;
	if (event == nullptr || event->structSize < D2RL::ItemInteractions::ItemInteractionEventRequiredSize
		|| event->action != D2RL::ItemInteractions::Action::Activate) {
		return Decision::Continue;
	}
	bool townPortal = false, potions = false, questItems = false, respecs = false;
	int difficulty = 0;
	{
		std::lock_guard lock(gMutex);
		townPortal = gLocks.townPortal;
		potions = gLocks.potions;
		questItems = gSlots.questItemChecks && gDifficultyKnown;
		respecs = gSlots.randomizer.treeMode && gGameRules != nullptr && !gSave.unlockedSkills.empty();
		difficulty = gDifficulty;
	}
	if (!townPortal && !potions && !questItems && !respecs) {
		return Decision::Continue;
	}
	D2RL::Items::ItemInfo info { .structSize = D2RL::Items::ItemInfoSize };
	if (gItems->getItemInfo(gContext, event->item, &info) != D2RL::Items::Result::Success) {
		return Decision::Continue;
	}
	{
		std::string code(4, ' ');
		std::memcpy(code.data(), &info.code, 4);
		Log("Archipelago: activated \"" + code + "\" (container " + std::to_string(static_cast<uint32_t>(event->container)) + ", input "
			+ std::to_string(static_cast<uint32_t>(event->inputSource)) + ", modifiers " + std::to_string(event->modifiers) + ", buttons " + (MouseButtonDown(VK_LBUTTON) ? "L" : "")
			+ (MouseButtonDown(VK_RBUTTON) ? "R" : "") + ").");
	}
	if (IsItemMove(*event)) {
		return Decision::Continue;
	}
	if (questItems && !IsReward(info)) {
		if (const auto check = QuestItemCheck(info.code, difficulty)) {
			std::lock_guard lock(gMutex);
			gCollectionQueue.push_back(*check);
		}
	}
	// A token only works from the inventory; anywhere else the click doesn't use it.
	const bool token = info.code == ap::loot::CodeFromText("toa") || info.code == ap::loot::CodeFromText(ap::randomizer::RespecCode);
	if (respecs && token && event->container == D2RL::Items::ItemContainer::Inventory) {
		std::lock_guard lock(gMutex);
		gRespecToken = event->item;
		gRespecTokenAt = Clock::now();
	}
	if (townPortal && (info.code == ap::loot::CodeFromText("tsc") || info.code == ap::loot::CodeFromText("tbk"))) {
		LockMessage("Town Portal is locked until you find Town Portal Access.");
		return Decision::Consume;
	}
	if (potions && IsPotion(info.code)) {
		LockMessage("potions are locked until you find Potion Access.");
		return Decision::Consume;
	}
	return Decision::Continue;
}

// With Enforce Act Keys, an area in a locked act sends you to the town of the
// furthest act you may be in, below it.
void EnforceActKeys(int difficulty, int level) {
	const ap::SlotData slots = gClient->Slots();
	if (!slots.actKeys || !slots.enforceActKeys || level < 0 || level >= static_cast<int>(std::size(ap::data::LevelActs))) {
		return;
	}
	const int act = ap::data::LevelActs[level];
	const std::set<int> unlocked = UnlockedActs();
	auto open = [&](int a) { return difficulty * 5 + a == 0 || unlocked.contains(difficulty * 5 + a); };
	if (act >= 5 || open(act)) {
		return;
	}
	int town = 0;
	for (int a = act - 1; a > 0; --a) {
		if (open(a)) {
			town = a;
			break;
		}
	}
	{
		std::lock_guard lock(gMutex);
		if (Clock::now() - gLastActWarp < std::chrono::seconds(3)) {
			return;
		}
		gLastActWarp = Clock::now();
		gCommandQueue.push_back("goto " + std::to_string(TownLevels[town]));
	}
	Feed("Archipelago: " + ActLabel(difficulty * 5 + act) + " is locked until you find its key; back to " + ActNames[town] + "'s town.");
}

// ---- Hints found in game -------------------------------------------------------
//
// A found hint either tells you where one of your important items is (the
// seed's hint list, act keys first) or, hint_balance percent of the time,
// which of your locations holds another player's important item. Both are
// registered with the server without spending hint points.

auto HintRandom() -> std::mt19937& {
	static std::mt19937 random(static_cast<uint32_t>(Clock::now().time_since_epoch().count()));
	return random;
}

auto OwnHintText(const ap::HintEntry& hint) -> std::string {
	const int me = gClient->MySlot();
	const std::string where = hint.player == me ? "here in your world" : "in " + gClient->PlayerName(hint.player) + "'s world";
	return "your " + gClient->ItemName(hint.item, me) + " is at " + gClient->LocationName(hint.location, hint.player) + ", " + where + ".";
}

auto OtherHintText(int64_t location, const ap::NetworkItem& item) -> std::string {
	return gClient->LocationName(location, gClient->MySlot()) + " holds " + gClient->PlayerName(item.player) + "'s "
	     + gClient->ItemName(item.item, item.player) + ".";
}

void RevealHint(const std::string& flavor) {
	const ap::SlotData slots = gClient->Slots();
	const int me = gClient->MySlot();
	const std::set<int64_t> checked = gClient->Checked();
	const auto scouted = gClient->Scouted();
	std::optional<int> own;
	std::vector<std::pair<int64_t, ap::NetworkItem>> progression, others;
	{
		std::lock_guard lock(gMutex);
		for (int index = 0; index < static_cast<int>(slots.hints.size()) && !own; ++index) {
			const auto& hint = slots.hints[static_cast<size_t>(index)];
			const bool found = hint.player == me && (checked.contains(hint.location) || gSave.checks.contains(hint.location));
			if (!gSave.ownHints.contains(index) && !found) {
				own = index;
			}
		}
		for (const auto& [location, item] : scouted) {
			if (item.player != me && !checked.contains(location) && !gSave.checks.contains(location) && !gSave.otherHints.contains(location)) {
				((item.flags & 1) != 0 ? progression : others).emplace_back(location, item);
			}
		}
	}
	const auto& pool = progression.empty() ? others : progression;
	const bool other = !pool.empty() && (!own || static_cast<int>(HintRandom()() % 100) < slots.hintBalance);
	std::string text = "the page is blank; there's nothing left to tell.";
	if (other) {
		const auto& [location, item] = pool[HintRandom()() % pool.size()];
		{
			std::lock_guard lock(gMutex);
			gSave.otherHints.insert(location);
			gSave.Save(gSavePath);
		}
		gClient->HintOwnLocation(location);
		text = OtherHintText(location, item);
	} else if (own) {
		const auto& hint = slots.hints[static_cast<size_t>(*own)];
		{
			std::lock_guard lock(gMutex);
			gSave.ownHints.insert(*own);
			gSave.Save(gSavePath);
		}
		gClient->CreateHint(hint.location, hint.player);
		text = OwnHintText(hint);
	}
	Feed("Archipelago: " + flavor + " " + text);
}

// ---- Banners and the status line ----------------------------------------------
//
// D2RLoader's overlay draws nothing on 1.3.1-beta, so these are windows of our
// own (notify.cpp); OnFrame, the overlay callback, only drives them.

void __cdecl BannerWork(const D2RL::PluginContext*, void*) noexcept {
	gBannerQueued = false;
	ap::notify::Tick();
}

// A banner for each item received since the last one announced: act keys,
// locks and skills in gold, traps in red.
void AnnounceItems() {
	static uint64_t seen = UINT64_MAX;
	const uint64_t generation = gClient->ItemsGeneration();
	size_t from = 0;
	{
		std::lock_guard lock(gMutex);
		if (gBoundSession == 0 || generation == seen) {
			return;
		}
		from = gSave.notifiedItems;
	}
	seen = generation;
	const auto items = gClient->Items();
	if (from >= items.size()) {
		return;
	}
	const int me = gClient->MySlot();
	// A big catch-up (reconnecting, a new save) is one line. Otherwise keys,
	// locks, skills and traps each get a banner, and ordinary loot is summed up
	// when more than three arrive together.
	if (items.size() - from > 10) {
		Banner("Received " + std::to_string(items.size() - from) + " items", ap::notify::Style::Info);
	} else {
		std::vector<std::pair<std::string, ap::notify::Style>> ordinary;
		for (size_t i = from; i < items.size(); ++i) {
			const auto& item = items[i];
			using ap::loot::Loot;
			const auto loot = static_cast<Loot>(item.item - ap::ids::LootBase);
			const bool trap = ap::ids::IsLootItem(item.item)
			               && (loot == Loot::JunkBackpack || (loot >= Loot::AmbushTrap && loot <= Loot::MercenaryDownTrap));
			const bool important = ap::ids::IsKeyItem(item.item) || ap::ids::IsLockItem(item.item) || ap::ids::IsSkillItem(item.item)
			                    || item.item == ap::ids::EmpowermentItem || item.item == ap::ids::SkillPointItem;
			std::string text = "Received: " + gClient->ItemName(item.item, me);
			if (item.player > 0 && item.player != me) {
				text += " from " + gClient->PlayerName(item.player);
			}
			if (trap || important) {
				Banner(text, trap ? ap::notify::Style::Trap : ap::notify::Style::Important);
			} else {
				ordinary.emplace_back(std::move(text), ap::notify::Style::Info);
			}
		}
		if (ordinary.size() > 3) {
			Banner("Received " + std::to_string(ordinary.size()) + " more items", ap::notify::Style::Info);
		} else {
			for (const auto& [text, style] : ordinary) {
				Banner(text, style);
			}
		}
	}
	std::lock_guard lock(gMutex);
	gSave.notifiedItems = items.size();
	gSave.Save(gSavePath);
}

auto StatusLine() -> std::string;

// The always-on checks line, refreshed once a second while in a game.
void UpdateStatusLine() {
	static Clock::time_point next;
	if (Clock::now() < next) {
		return;
	}
	next = Clock::now() + std::chrono::seconds(1);
	bool show = false;
	std::string position;
	{
		std::lock_guard lock(gMutex);
		show = gInGame && gConfig.statusLine;
		position = gConfig.statusPosition;
	}
	using Corner = ap::notify::Corner;
	static const std::map<std::string, Corner> corners {
		{ "top-left", Corner::TopLeft }, { "top", Corner::Top }, { "top-right", Corner::TopRight },
		{ "bottom-left", Corner::BottomLeft }, { "bottom-right", Corner::BottomRight } };
	const auto corner = corners.find(position);
	ap::notify::SetStatus(show ? StatusLine() : std::string(), corner != corners.end() ? corner->second : Corner::TopLeft);
}

// ap-debug overlay: test shapes for finding out why the overlay doesn't show.
// Each colour tests one idea; the log gets each draw call's result.
std::atomic<bool> gOverlayTest;
uint64_t          gOverlayTestLogged = 0;

void DrawOverlayTest(const D2RL::Overlay::Frame* frame) {
	const float w = frame->screenWidth, h = frame->screenHeight;
	std::string results;
	auto fill = [&](const char* name, D2RL::Overlay::Rect rect, D2RL::Overlay::Color color) {
		const D2RL::Overlay::FilledRectangleRequest request {
			.structSize = D2RL::Overlay::FilledRectangleRequestSize,
			.canvas     = frame->canvas,
			.rect       = rect,
			.color      = color,
		};
		results += std::string(" ") + name + "=" + std::to_string(static_cast<uint32_t>(gOverlay->drawFilledRectangle(gContext, &request)));
	};
	// Inside a 32x32 window at the corner (the size imgui.ini had saved).
	fill("white-corner", { 2, 2, 30, 30 }, { 1, 1, 1, 1 });
	// Screen-relative pixels.
	fill("red-relative", { w * 0.10F, h * 0.10F, w * 0.25F, h * 0.25F }, { 1, 0, 0, 1 });
	// Fixed pixels.
	fill("green-fixed", { 400, 400, 800, 800 }, { 0, 1, 0, 1 });
	// 0..1 coordinates.
	fill("blue-normalized", { 0.40F, 0.10F, 0.55F, 0.25F }, { 0, 0, 1, 1 });
	// 0..255 colours.
	fill("magenta-255", { w * 0.60F, h * 0.10F, w * 0.75F, h * 0.25F }, { 255, 0, 255, 255 });
	const D2RL::Overlay::RectangleRequest border {
		.structSize = D2RL::Overlay::RectangleRequestSize,
		.canvas     = frame->canvas,
		.rect       = { 10, 10, w - 10, h - 10 },
		.color      = { 1, 0.5F, 0, 1 },
		.thickness  = 8,
	};
	results += " orange-border=" + std::to_string(static_cast<uint32_t>(gOverlay->drawRectangle(gContext, &border)));
	const std::string text = "ARCHIPELAGO OVERLAY TEST " + std::to_string(static_cast<int>(w)) + "x" + std::to_string(static_cast<int>(h));
	const D2RL::Overlay::TextRequest request {
		.structSize = D2RL::Overlay::TextRequestSize,
		.canvas     = frame->canvas,
		.position   = { w * 0.10F, h * 0.30F },
		.color      = { 1, 1, 0, 1 },
		.textSize   = 64,
		.text       = text.data(),
		.textLength = static_cast<uint32_t>(text.size()),
	};
	results += " yellow-text=" + std::to_string(static_cast<uint32_t>(gOverlay->drawText(gContext, &request)));
	// The first frames after turning it on, then every 10 seconds or so.
	if (gOverlayTestLogged < 3 || frame->frameNumber % 600 == 0) {
		++gOverlayTestLogged;
		D2RL::Overlay::Metrics metrics { .structSize = D2RL::Overlay::MetricsSize };
		const auto metricsResult = gOverlay->getMetrics(gContext, &metrics);
		Log("Archipelago: overlay test frame " + std::to_string(frame->frameNumber) + ", screen " + std::to_string(w) + "x" + std::to_string(h) + ", text size "
		    + std::to_string(frame->defaultTextSize) + ", metrics " + std::to_string(static_cast<uint32_t>(metricsResult)) + " (" + std::to_string(metrics.screenWidth)
		    + "x" + std::to_string(metrics.screenHeight) + "); results (0 = success):" + results);
	}
}

void __cdecl OnFrame(const D2RL::PluginContext*, const D2RL::Overlay::Frame* frame, void*) noexcept {
	if (frame == nullptr || !gClient) {
		return;
	}
	if (static bool first = true; first) {
		first = false;
		Log("Archipelago: first overlay frame: screen " + std::to_string(frame->screenWidth) + "x" + std::to_string(frame->screenHeight) + ", struct size "
		    + std::to_string(frame->structSize) + ".");
	}
	if (gOverlayTest) {
		DrawOverlayTest(frame);
	}
	AnnounceItems();
	UpdateStatusLine();
	if (ap::notify::Busy() && !gBannerQueued.exchange(true) && gThreads->runOnUiThread(gContext, BannerWork, nullptr) != D2RL::Threads::Result::Success) {
		gBannerQueued = false;
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

	std::vector<int> quests;
	int questDifficulty = -1;
	{
		std::lock_guard lock(gMutex);
		quests.swap(gQuestQueue);
		if (gDifficultyKnown) {
			questDifficulty = std::clamp(gDifficulty, 0, 2);
		}
	}
	// An area trigger waiting for the difficulty holds back everything after it, to keep the order.
	std::vector<int> waiting;
	for (const int quest : quests) {
		int difficulty = quest % 1000 / 100;
		if (!waiting.empty() || (difficulty == CurrentDifficulty && questDifficulty < 0)) {
			waiting.push_back(quest);
			continue;
		}
		if (difficulty == CurrentDifficulty) {
			difficulty = questDifficulty;
		}
		if (static_cast<QuestSource>(quest / 1000) == QuestSource::Event) {
			Log("Archipelago: quest event for row " + std::to_string(quest % 100) + " on difficulty " + std::to_string(difficulty) + ".");
		}
		CompleteQuest(static_cast<uint32_t>(difficulty), static_cast<uint32_t>(quest % 100), static_cast<QuestSource>(quest / 1000));
	}
	if (!waiting.empty()) {
		std::lock_guard lock(gMutex);
		gQuestQueue.insert(gQuestQueue.begin(), waiting.begin(), waiting.end());
	}
	std::vector<int> areas;
	int difficulty = 0;
	bool bound = false;
	{
		std::lock_guard lock(gMutex);
		if (gDifficultyKnown) {
			areas.swap(gPendingAreas);
			difficulty = std::clamp(gDifficulty, 0, 2);
		}
		bound = gBoundSession != 0;
	}
	const std::set<int64_t> sideAreas = areas.empty() ? std::set<int64_t> {} : gClient->Slots().sideAreas;
	UpdateLocks();
	for (const int level : areas) {
		EnforceActKeys(difficulty, level);
		if (level == ap::data::CowLevel) {
			std::lock_guard lock(gMutex);
			gCowQueue.insert(difficulty * 10);
		}
		for (const auto& area : ap::data::Areas) {
			if (area.id == level) {
				AddChecks({ ap::ids::AreaLocation(static_cast<uint32_t>(difficulty), area.id) });
			}
		}
		// Only the side areas the seed picked; before a seed is bound, keep them all (the server skips the rest).
		for (const auto& area : ap::data::SideAreas) {
			const int64_t location = ap::ids::AreaLocation(static_cast<uint32_t>(difficulty), area.id);
			if (area.id == level && (!bound || sideAreas.contains(location))) {
				AddChecks({ location });
			}
		}
	}
	std::set<int> cows;
	std::vector<int64_t> collections;
	bool commands = false;
	{
		std::lock_guard lock(gMutex);
		cows.swap(gCowQueue);
		collections.swap(gCollectionQueue);
		commands = (!gCommandQueue.empty() && Clock::now() >= gNextCommandsAt)
		        || (!gTrapQueue.empty() && gInGame && Clock::now() >= gNextTrapAt && (!gSlots.traps.waitForTown || !IsTown(gCurrentLevel)));
	}
	if (gClient->Slots().cowLevelChecks) {
		for (const int cow : cows) {
			collections.push_back(ap::ids::CowLocation(static_cast<uint32_t>(cow / 10), static_cast<uint32_t>(cow % 10)));
		}
	}
	AddChecks(collections);
	if (commands && !gCommandsQueued.exchange(true) && gThreads->runOnUiThread(gContext, RunCommands, nullptr) != D2RL::Threads::Result::Success) {
		gCommandsQueued = false;
	}
	std::set<int> superUniques;
	{
		std::lock_guard lock(gMutex);
		superUniques.swap(gSuperUniqueQueue);
	}
	for (const int kill : superUniques) {
		const int64_t location = ap::ids::SuperUniqueLocation(static_cast<uint32_t>(kill / 100), static_cast<uint32_t>(kill % 100));
		const int chance = gClient->Slots().pageHintChance;
		if (chance > 0 && IsNewCheck(location) && static_cast<int>(HintRandom()() % 100) < chance) {
			std::string name = "the superunique";
			for (const auto& entry : ap::data::SuperUniques) {
				// The label is the name the game shows, then the area in brackets.
				const std::string_view label = entry.label;
				name = entry.id == kill % 100 ? std::string(label.substr(0, label.find(" ("))) : name;
			}
			std::lock_guard lock(gMutex);
			gHintQueue.push_back("A torn page falls from " + name + ":");
		}
		AddChecks({ location });
	}
	std::vector<std::string> hints;
	{
		std::lock_guard lock(gMutex);
		if (gBoundSession != 0 && gClient->State() == ap::ConnectionState::Connected) {
			hints.swap(gHintQueue);
		}
	}
	for (const auto& flavor : hints) {
		RevealHint(flavor);
	}
	for (const std::string& death : gClient->TakeDeaths()) {
		Feed("Archipelago: DeathLink: " + death);
		std::lock_guard lock(gMutex);
		// Deaths while you're out of a game are let go.
		gDeathPending = gInGame;
	}
	bool kill = false;
	{
		std::lock_guard lock(gMutex);
		kill = gDeathPending && gInGame;
	}
	if (kill && !gKillQueued.exchange(true) && gThreads->runOnUiThread(gContext, KillPlayer, nullptr) != D2RL::Threads::Result::Success) {
		gKillQueued = false;
	}

	bool work = false;
	{
		std::lock_guard lock(gMutex);
		const bool scanDue = Clock::now() >= gNextQuestScan;
		if (scanDue) {
			gNextQuestScan = Clock::now() + std::chrono::seconds(5);
		}
		work = gInGame && gBoundSession != 0 && (scanDue || ((gCodexDirty || gCodexItems != gClient->ItemsGeneration()) && Clock::now() >= gRetryAt));
	}
	bool console = false;
	{
		std::lock_guard lock(gMutex);
		console = !gConsoleQueue.empty() && Clock::now() >= gNextFlushAt;
	}
	if (console && !gConsoleQueued.exchange(true) && gThreads->runOnUiThread(gContext, FlushConsole, nullptr) != D2RL::Threads::Result::Success) {
		gConsoleQueued = false;
	}
	if (work) {
		const int64_t now = Clock::now().time_since_epoch().count();
		bool queue = !gWorkQueued.exchange(true);
		if (!queue && Clock::duration(now - gWorkQueuedAt) > std::chrono::seconds(15)) {
			Log("Archipelago: queued game work never ran; queuing it again.");
			queue = true;
		}
		if (queue) {
			gWorkQueuedAt = now;
			if (gThreads->runOnGameThread(gContext, GameWork, nullptr) != D2RL::Threads::Result::Success) {
				gWorkQueued = false;
			}
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
			gWorkQueued = false;
			std::lock_guard lock(gMutex);
			gInGame = true;
			gRepairOffer.Cancel();
			gRepairOfferQueued = false;
			gCodexDirty = true;
			gPlayerDead = false;
			gPlayerReadyAt = Clock::now();
			gDifficultyKnown = false;
			gPendingAreas.clear();
			DropUnresolvedAreaQuests();
			// The old game's item handles are stale, which would look like a used token.
			gRespecToken = D2RL::InvalidItemHandle;
			break;
		}
		case Kind::PlayerResurrected: {
			// The death screen's UI message should come first; if it didn't, the
			// resurrection still means a death (but not right after joining).
			bool missed = false;
			{
				std::lock_guard lock(gMutex);
				missed = !gPlayerDead && Clock::now() - gPlayerReadyAt > std::chrono::seconds(10);
			}
			Log(std::string("Archipelago: resurrected") + (missed ? " without a death message." : "."));
			if (missed) {
				PlayerDied("resurrected");
			}
			std::lock_guard lock(gMutex);
			gPlayerDead = false;
			break;
		}
		case Kind::GameLeft: {
			gWorkQueued = false;
			std::lock_guard lock(gMutex);
			gInGame = false;
			gRepairOffer.Cancel();
			gRepairOfferQueued = false;
			gRespecToken = D2RL::InvalidItemHandle;
			break;
		}
		case Kind::QuestCompleted: {
			// The row should be the quest flag in data::Quests (1 = Den of Evil);
			// rows we don't check (act intros, act completions) only log their row.
			// It joins the fallbacks' queue, and logs from there, so the log keeps the order they happened in.
			if (event->difficulty < 3 && event->questRecordId < 100) {
				std::lock_guard lock(gMutex);
				// Unlike LevelChanged, the quest event's difficulty is always set (0 is Normal).
				gDifficulty = static_cast<int>(event->difficulty);
				gDifficultyKnown = true;
				gQuestQueue.push_back(static_cast<int>(QuestSource::Event) * 1000 + static_cast<int>(event->difficulty) * 100 + static_cast<int>(event->questRecordId));
			} else {
				Log("Archipelago: quest event for row " + std::to_string(event->questRecordId) + " on difficulty " + std::to_string(event->difficulty) + ".");
			}
			break;
		}
		case Kind::LevelChanged: {
			std::lock_guard lock(gMutex);
			gRepairOffer.Cancel();
			// A nonzero difficulty can only be real; zero may just mean unset.
			if (event->difficulty > 0 && event->difficulty < 3) {
				gDifficulty = static_cast<int>(event->difficulty);
				gDifficultyKnown = true;
			}
			gPendingAreas.push_back(event->currentValue);
			// Queued with the other quest sources, so the log keeps their order.
			if (const auto flag = ap::quests::Find(ap::quests::Trigger::Area, static_cast<uint32_t>(event->currentValue))) {
				gQuestQueue.push_back(static_cast<int>(QuestSource::Area) * 1000 + CurrentDifficulty * 100 + *flag);
			}
			gCurrentLevel = event->currentValue;
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

// The Ledger's tooltip: checks done (sent, or held for a key) out of each act's
// total, with the act you're in marked, then the ones not tied to an act.
// Checks done and total per act (-1 for none), with the act you're in and how
// many found checks are held for a missing act key.
struct CheckCounts {
	std::map<int, std::pair<size_t, size_t>> acts;
	size_t done = 0, total = 0, held = 0;
	int    here = 0;
};

auto CountChecks() -> CheckCounts {
	CheckCounts counts;
	const ap::SlotData slots = gClient->Slots();
	std::set<int64_t> found;
	{
		std::lock_guard lock(gMutex);
		found = gBoundSession != 0 ? gSave.checks : gUnbound;
		counts.here = std::clamp(gDifficulty, 0, 2) * 5 + std::clamp(gAct, 0, 4);
	}
	for (const int64_t location : gClient->Checked()) {
		auto& [done, total] = counts.acts[ActOfLocation(location, slots)];
		++done;
		++total;
	}
	for (const int64_t location : gClient->Missing()) {
		auto& [done, total] = counts.acts[ActOfLocation(location, slots)];
		++total;
		if (found.contains(location)) {
			++done;
			++counts.held;
		}
	}
	for (const auto& [act, pair] : counts.acts) {
		counts.done += pair.first;
		counts.total += pair.second;
	}
	return counts;
}

auto LedgerText() -> std::string {
	if (gClient->State() != ap::ConnectionState::Connected) {
		return "Archipelago: not connected";
	}
	const CheckCounts counts = CountChecks();
	auto line = [](const std::string& label, const std::pair<size_t, size_t>& pair) {
		return "\n" + label + ": " + std::to_string(pair.first) + "/" + std::to_string(pair.second) + (pair.first == pair.second ? " done" : "");
	};
	std::string text = "Archipelago: " + std::to_string(counts.done) + "/" + std::to_string(counts.total) + " checks";
	for (const auto& [act, pair] : counts.acts) {
		if (act >= 0) {
			text += line((act == counts.here ? "> " : "") + ActLabel(act), pair);
		}
	}
	if (const auto other = counts.acts.find(-1); other != counts.acts.end()) {
		text += line("Not tied to an act", other->second);
	}
	if (counts.held > 0) {
		text += "\n" + std::to_string(counts.held) + " held until their act's key arrives";
	}
	return text;
}

// The always-on status line: "Archipelago 34/210 - Normal Act I 12/40".
auto StatusLine() -> std::string {
	if (gClient->State() != ap::ConnectionState::Connected) {
		return "Archipelago: not connected";
	}
	const CheckCounts counts = CountChecks();
	std::string text = "Archipelago " + std::to_string(counts.done) + "/" + std::to_string(counts.total);
	if (const auto act = counts.acts.find(counts.here); act != counts.acts.end()) {
		text += "  -  " + ActLabel(counts.here) + " " + std::to_string(act->second.first) + "/" + std::to_string(act->second.second);
	}
	if (counts.held > 0) {
		text += "  -  " + std::to_string(counts.held) + " held";
	}
	return text;
}

// Hovering over the Codex shows the connection and the latest messages, so you
// don't have to open the console.
void __cdecl OnItemTooltip(const D2RL::PluginContext*, D2RL::SharedEvents::ItemTooltipEvent* event, void*) noexcept {
	if (event == nullptr || event->structSize < D2RL::SharedEvents::ItemTooltipEventRequiredSize || event->text == nullptr || !gClient) {
		return;
	}
	D2RL::Items::ItemInfo info { .structSize = D2RL::Items::ItemInfoSize };
	if (gItems->getItemInfo(gContext, event->item, &info) != D2RL::Items::Result::Success || event->capacity < 2) {
		return;
	}
	if (info.code == ap::loot::CodeFromText(ap::randomizer::LedgerCode)) {
		const std::string text = LedgerText();
		const size_t length = std::min<size_t>(text.size(), event->capacity - 1);
		std::memcpy(event->text, text.data(), length);
		event->text[length] = '\0';
		event->length = static_cast<uint32_t>(length);
		return;
	}
	// The Codex is the Archipelago Codex unique; a fallback Magic Codex keeps its seed marker.
	bool codex = CodexSeedOf(info) != 0;
	if (info.quality == D2RL::Items::Quality::Unique && info.qualityRecordId >= 0) {
		std::lock_guard lock(gMutex);
		codex = codex
		     || (info.code == ap::loot::CodeFromText(gConfig.codexItem.c_str())
		         && std::ranges::find(gCodexRecords, static_cast<uint32_t>(info.qualityRecordId)) != gCodexRecords.end());
	}
	if (!codex) {
		return;
	}
	// Newest message first, as many as fit. (D2's color markers show as plain
	// text in tooltip additions, so there are none.)
	std::string text = "Archipelago: ";
	text += gClient->State() == ap::ConnectionState::Connected
	          ? std::to_string(gClient->CheckedCount()) + "/" + std::to_string(gClient->LocationCount()) + " checks"
	          : std::string("not connected");
	{
		std::lock_guard lock(gMutex);
		for (auto line = gHistory.rbegin(); line != gHistory.rend(); ++line) {
			std::string shown = line->rfind("Archipelago: ", 0) == 0 ? line->substr(13) : *line;
			// Server housekeeping isn't worth the space.
			if (shown.find("compressed websocket") != std::string::npos || shown.find(" has joined") != std::string::npos
			    || shown.find(" has left the game") != std::string::npos) {
				continue;
			}
			// Tooltips don't wrap; keep lines short.
			if (shown.size() > 70) {
				shown = shown.substr(0, 67) + "...";
			}
			const std::string next = "\n" + shown;
			if (text.size() + next.size() + 1 > event->capacity) {
				break;
			}
			text += next;
		}
	}
	const size_t length = std::min<size_t>(text.size(), event->capacity - 1);
	std::memcpy(event->text, text.data(), length);
	event->text[length] = '\0';
	event->length = static_cast<uint32_t>(length);
}

void RunPanelButton(std::string_view id);

// Watches for D2R's death screen (YouDiedModal), runs the command panel's
// buttons, and logs each distinct UI message once, to find ones worth reacting to.
auto __cdecl OnUiMessage(const D2RL::PluginContext*, const D2RL::SharedEvents::UiMessageEvent* event, void*) noexcept -> D2RL::SharedEvents::UiMessageAction {
	static std::set<std::string> seen;
	if (event == nullptr || event->structSize < D2RL::SharedEvents::UiMessageEventRequiredSize || event->target == nullptr || event->command == nullptr) {
		return D2RL::SharedEvents::UiMessageAction::Continue;
	}
	const std::string key = std::string(event->target) + ":" + event->command;
	if (std::string_view(event->target) == "VendorPanelMessage") {
		bool locked = false;
		{
			std::lock_guard lock(gMutex);
			locked = gLocks.merchants;
		}
		if (locked && !(gNativeRepairDispatch && std::string_view(event->command) == "RepairAll")
			&& std::string_view(event->command) != "Close") {
			LockMessage("shopping is locked until you find Merchant Access.");
			return D2RL::SharedEvents::UiMessageAction::Consume;
		}
	}
	std::string lower = key + ":" + (event->text != nullptr ? event->text : "");
	std::ranges::transform(lower, lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	if (lower.find("youdied") != std::string::npos || lower.find("playerdied") != std::string::npos) {
		PlayerDied(key);
	}
	if (const auto id = ap::panel::ClickedButton(event->target, event->command, event->text != nullptr ? event->text : ""); !id.empty()) {
		RunPanelButton(id);
		return D2RL::SharedEvents::UiMessageAction::Consume;
	}
	if (std::string_view(event->target) == "PanelManager" && event->text != nullptr
		&& (std::string_view(event->command) == "OpenPanel" || std::string_view(event->command) == "OpenExclusivePanel"
			|| std::string_view(event->command) == "TogglePanel")) {
		if (const std::string reason = LockedPanel(event->text); !reason.empty()) {
			if (std::string_view(event->command) != "TogglePanel" && ap::repairs::IsVendorPanel(event->text) && OfferRepairs(event->text)) {
				Log("Archipelago: blocked vendor panel; offering experimental repairs.");
				return D2RL::SharedEvents::UiMessageAction::Consume;
			}
			LockMessage(reason);
			return D2RL::SharedEvents::UiMessageAction::Consume;
		}
	}
	if (std::string_view(event->target) == "PanelManager") {
		const std::string_view command = event->command;
		const std::string_view panel = event->text == nullptr ? std::string_view() : ap::repairs::PanelName(event->text);
		if ((command == "ClosePanel" && panel == ap::panel::Repairs().name)
			|| ((command == "OpenPanel" || command == "OpenExclusivePanel" || command == "TogglePanel")
				&& panel != ap::panel::Repairs().name)) {
			std::lock_guard lock(gMutex);
			gRepairOffer.Cancel();
		}
	}
	if (seen.size() >= 400) {
		return D2RL::SharedEvents::UiMessageAction::Continue;
	}
	// Panel names matter (which panel the stash or a merchant opens), so those log once per panel.
	if (seen.insert(key == "PanelManager:OpenPanel" || key == "PanelManager:OpenExclusivePanel" || key == "PanelManager:TogglePanel"
		    ? key + ":" + (event->text != nullptr ? event->text : "") : key).second) {
		const std::string text = event->text != nullptr ? std::string(event->text).substr(0, 80) : "";
		Log("Archipelago: UI message " + key + (text.empty() ? "" : " \"" + text + "\""));
	}
	return D2RL::SharedEvents::UiMessageAction::Continue;
}

// Runs on the server thread: the current difficulty, quest bosses, Den of Evil
// kills, field superuniques and the player's class.
void __cdecl OnMonsterDeath(const D2RL::PluginContext*, const D2RL::Lifecycle::MonsterDeathEvent* event, void*) noexcept {
	if (!D2RL::Lifecycle::HasMonsterDeathEventField(event, D2RL::Lifecycle::MonsterDeathEventRequiredSize)) {
		return;
	}
	std::lock_guard lock(gMutex);
	gDifficulty = static_cast<int>(event->difficulty);
	gDifficultyKnown = true;
	const int difficulty = std::clamp(gDifficulty, 0, 2);
	if (const auto flag = ap::quests::Find(ap::quests::Trigger::Kill, event->monster.classId)) {
		gQuestQueue.push_back(static_cast<int>(QuestSource::Kill) * 1000 + difficulty * 100 + *flag);
	}
	if (event->levelId == static_cast<int32_t>(ap::quests::DenLevel) && ++gDenKills[difficulty] == ap::quests::DenKills) {
		gQuestQueue.push_back(static_cast<int>(QuestSource::DenKills) * 1000 + difficulty * 100 + 1);
	}
	const int monster = static_cast<int>(event->monster.classId);
	for (uint32_t index = 0; gSlots.uberChecks && difficulty == 2 && index < std::size(ap::data::Ubers); ++index) {
		if (monster == ap::data::Ubers[index].monster) {
			gCollectionQueue.push_back(ap::ids::UberLocation(index));
		}
	}
	if (gSlots.killCountChecks && gBoundSession != 0 && event->levelId >= 0 && event->levelId < static_cast<int32_t>(std::size(ap::data::LevelActs))) {
		const int act = ap::data::LevelActs[event->levelId];
		if (act < 5) {
			const int count = ++gSave.kills[difficulty * 10 + act];
			bool milestone = false;
			for (uint32_t index = 0; index < std::size(ap::ids::KillCounts); ++index) {
				if (count == static_cast<int>(ap::ids::KillCounts[index])) {
					gCollectionQueue.push_back(ap::ids::KillLocation(static_cast<uint32_t>(difficulty), static_cast<uint32_t>(act), index));
					milestone = true;
				}
			}
			if (milestone || count % 25 == 0) {
				gSave.Save(gSavePath);
			}
		}
	}
	if (monster == ap::data::CowKingMonster) {
		gCowQueue.insert(difficulty * 10 + 1);
	} else if (monster == ap::data::CowMonster) {
		int& cows = gBoundSession != 0 ? gSave.cowKills[difficulty] : gCowKills[difficulty];
		if (++cows == static_cast<int>(ap::ids::CowKills)) {
			gCowQueue.insert(difficulty * 10 + 2);
		}
		if (gBoundSession != 0 && cows % 25 == 0) {
			gSave.Save(gSavePath);
		}
	}
	for (const int base : gSuperUniqueBase) {
		if (gSuperUniqueMod && base >= 0 && monster >= base && monster - base < static_cast<int>(std::size(ap::data::SuperUniques))) {
			gSuperUniqueQueue.insert(difficulty * 100 + ap::data::SuperUniques[monster - base].id);
			break;
		}
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
	{
		std::lock_guard lock(gMutex);
		gRepairOffer.Cancel();
	}
	gClient->Disconnect();
	Reply("Disconnected from Archipelago.");
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
	Reply(text);
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

// ap-core <command>: runs a D2RCore console command the way rewards do (on the
// next frame in game), for trying commands such as skadd.
auto CommandCore(D2R::Game::Client*, const D2RL::ConsoleCommandContext* command, void*) noexcept -> D2RL::ConsoleCommandResult {
	const std::string text(command->args != nullptr ? command->args : "", command->argsLength);
	if (text.empty()) {
		gContext->WriteConsoleMessage("Usage: ap-core <D2RCore command>, for example ap-core skadd Leap,1");
		return D2RL::ConsoleCommandResult::InvalidArguments;
	}
	std::lock_guard lock(gMutex);
	gCommandQueue.push_back(text);
	return D2RL::ConsoleCommandResult::Handled;
}

// ap-relearn [confirm]: unlocks the received tree skills again after a respec,
// which takes away the level skadd gave. Nothing reads a skill's level, and
// canAllocateSkill also says no for an unlocked skill with no unspent points,
// below its level requirement or at its cap, while skadd adds a level each
// time. So ap-relearn only lists the skills that won't take a point, and
// ap-relearn confirm (you just respecced) gives each of them a level back. A
// failed check changes nothing. A Token or Writ of Absolution does this on its own
// (RelearnAfterToken); Akara's respec can't be seen, so it needs ap-relearn.
bool gRelearnConfirmed = false;

void __cdecl RelearnWork(const D2RL::PluginContext*, void*) noexcept {
	auto say = [](const std::string& text) {
		std::lock_guard lock(gMutex);
		gConsoleQueue.push_back(text);
	};
	D2RL::PlayerHandle player = D2RL::InvalidPlayerHandle;
	if (gInventory->getLocalPlayer(gContext, &player) != D2RL::Inventory::Result::Success) {
		say("Archipelago: join a game first, then ap-relearn.");
		return;
	}
	if (gGameRules == nullptr) {
		say("Archipelago: ap-relearn needs a newer D2RLoader (its game rule service) to tell which skills a respec took.");
		return;
	}
	bool confirmed = false;
	{
		std::lock_guard lock(gMutex);
		confirmed = gRelearnConfirmed;
		gRelearnConfirmed = false;
	}
	const auto checked = RefusedSkills(player);
	if (!checked) {
		say("Archipelago: couldn't check your skills, so ap-relearn changed nothing.");
		return;
	}
	const std::vector<uint32_t>& refused = *checked;
	const std::string names = SkillNames(refused);
	if (refused.empty()) {
		say("Archipelago: all your Archipelago skills are still unlocked.");
		return;
	}
	if (!confirmed) {
		say("Archipelago: these skills won't take a point: " + names + ". After a respec that's expected, but it's the same with no unspent "
		    "skill points, below a skill's level requirement or at its cap. If you just respecced, type ap-relearn confirm.");
		return;
	}
	std::lock_guard lock(gMutex);
	for (const uint32_t skill : refused) {
		gCommandQueue.push_back("skadd " + SkillName(skill) + ",1");
	}
	gConsoleQueue.push_back("Archipelago: unlocked again: " + names + ".");
}

auto CommandRelearn(D2R::Game::Client*, const D2RL::ConsoleCommandContext* command, void*) noexcept -> D2RL::ConsoleCommandResult {
	const auto args = SplitArgs(command);
	{
		std::lock_guard lock(gMutex);
		gRelearnConfirmed = !args.empty() && args[0] == "confirm";
	}
	if (gThreads->runOnGameThread(gContext, RelearnWork, nullptr) != D2RL::Threads::Result::Success) {
		Reply("ap-relearn: join a game first.");
		return D2RL::ConsoleCommandResult::Failed;
	}
	return D2RL::ConsoleCommandResult::Handled;
}

// ap-personalize [name]: puts name on the item in your hand, the way rewards
// from other players get the sender's; with no name it only logs the item data.
std::string gPersonalizeName;

void __cdecl PersonalizeCursorWork(const D2RL::PluginContext*, void*) noexcept {
	std::string name;
	{
		std::lock_guard lock(gMutex);
		name = gPersonalizeName;
	}
	D2RL::PlayerHandle player = D2RL::InvalidPlayerHandle;
	D2RL::ItemHandle   item   = D2RL::InvalidItemHandle;
	std::string        line;
	if (gInventory->getLocalPlayer(gContext, &player) != D2RL::Inventory::Result::Success
		|| gInventory->getCursorItem(gContext, player, &item) != D2RL::Inventory::Result::Success || item == D2RL::InvalidItemHandle) {
		line = "ap-personalize: pick up an item first (hold it on the cursor).";
	} else if (const auto problem = Personalize(item, name, name.empty()); !problem.empty()) {
		line = "ap-personalize: " + problem + "; details in the log.";
	} else {
		line = name.empty() ? "ap-personalize: the item data matches the expected layout; it's in the log."
		                    : "ap-personalize: wrote " + name + ". Save and Exit, then rejoin, to see it.";
	}
	std::lock_guard lock(gMutex);
	gConsoleQueue.push_back(line);
}

auto CommandPersonalize(D2R::Game::Client*, const D2RL::ConsoleCommandContext* command, void*) noexcept -> D2RL::ConsoleCommandResult {
	const std::string text(command->args != nullptr ? command->args : "", command->argsLength);
	const std::string name = ap::personal::Name(text);
	if (!text.empty() && name.empty()) {
		gContext->WriteConsoleMessage("ap-personalize: that name has no characters D2 can show.");
		return D2RL::ConsoleCommandResult::InvalidArguments;
	}
	{
		std::lock_guard lock(gMutex);
		gPersonalizeName = name;
	}
	if (gThreads->runOnGameThread(gContext, PersonalizeCursorWork, nullptr) != D2RL::Threads::Result::Success) {
		gContext->WriteConsoleMessage("ap-personalize: join a game first.");
		return D2RL::ConsoleCommandResult::Failed;
	}
	return D2RL::ConsoleCommandResult::Handled;
}

// ap-chat <text>: show a line in D2R's chat, to test the chat output.
auto CommandChat(D2R::Game::Client*, const D2RL::ConsoleCommandContext* command, void*) noexcept -> D2RL::ConsoleCommandResult {
	const std::string text(command->args != nullptr ? command->args : "", command->argsLength);
	if (text.empty()) {
		return D2RL::ConsoleCommandResult::InvalidArguments;
	}
	if (ChatFunction() == nullptr) {
		gContext->WriteConsoleMessage("ap-chat: D2RCore has no DisplayServerChatMessage export.");
		return D2RL::ConsoleCommandResult::Failed;
	}
	ShowInChat(text);
	return D2RL::ConsoleCommandResult::Handled;
}

// ap-hints: every hint found in game for this seed.
auto CommandHints(D2R::Game::Client*, const D2RL::ConsoleCommandContext*, void*) noexcept -> D2RL::ConsoleCommandResult {
	const ap::SlotData slots = gClient->Slots();
	const auto scouted = gClient->Scouted();
	std::set<int> own;
	std::set<int64_t> others;
	{
		std::lock_guard lock(gMutex);
		own = gSave.ownHints;
		others = gSave.otherHints;
	}
	if (own.empty() && others.empty()) {
		Reply("No hints found yet: Deckard Cain, torn pages from superuniques and Scrolls of Prophecy have them.");
	}
	for (const int index : own) {
		if (index < static_cast<int>(slots.hints.size())) {
			Reply("Hint: " + OwnHintText(slots.hints[static_cast<size_t>(index)]));
		}
	}
	for (const int64_t location : others) {
		if (const auto item = scouted.find(location); item != scouted.end()) {
			Reply("Hint: " + OtherHintText(location, item->second));
		}
	}
	return D2RL::ConsoleCommandResult::Handled;
}

// ap-missing [all]: the checks left in the current act, or how many are left in every act.
// Checks found in a locked act wait for its key; they count as held, not missing.
auto CommandMissing(D2R::Game::Client*, const D2RL::ConsoleCommandContext* command, void*) noexcept -> D2RL::ConsoleCommandResult {
	if (gClient->State() != ap::ConnectionState::Connected) {
		Reply("Not connected: ap-missing needs the server's list of checks.");
		return D2RL::ConsoleCommandResult::Failed;
	}
	const auto args = SplitArgs(command);
	const bool all = !args.empty() && args[0] == "all";
	const ap::SlotData slots = gClient->Slots();
	std::set<int64_t> found;
	int here = 0;
	{
		std::lock_guard lock(gMutex);
		found = gBoundSession != 0 ? gSave.checks : gUnbound;
		here = std::clamp(gDifficulty, 0, 2) * 5 + std::clamp(gAct, 0, 4);
	}
	// Per act (-1 for checks that aren't tied to one): what's left, and how many are held.
	std::map<int, std::vector<int64_t>> left;
	std::map<int, int> held;
	for (const int64_t location : gClient->Missing()) {
		const int act = ActOfLocation(location, slots);
		if (found.contains(location)) {
			++held[act];
		} else {
			left[act].push_back(location);
		}
	}
	auto label = [](int act) { return act < 0 ? std::string("Any act") : ActLabel(act); };
	auto heldText = [&](int act) { return held[act] > 0 ? " (" + std::to_string(held[act]) + " found, not sent until its key arrives)" : std::string(); };
	if (all) {
		std::set<int> acts;
		for (const auto& [act, locations] : left) {
			acts.insert(act);
		}
		for (const auto& [act, count] : held) {
			acts.insert(act);
		}
		if (acts.empty()) {
			Reply("Every check is done.");
		}
		for (const int act : acts) {
			Reply(label(act) + ": " + std::to_string(left[act].size()) + " left" + heldText(act) + ".");
		}
		return D2RL::ConsoleCommandResult::Handled;
	}
	const int me = gClient->MySlot();
	Reply(ActLabel(here) + ": " + std::to_string(left[here].size()) + " left" + heldText(here) + ".");
	for (const int64_t location : left[here]) {
		Reply("  " + gClient->LocationName(location, me));
	}
	if (!left[-1].empty()) {
		Reply(std::to_string(left[-1].size()) + " more aren't tied to an act (level milestones, collections, find counts).");
	}
	Reply("ap-missing all counts what's left in every act.");
	return D2RL::ConsoleCommandResult::Handled;
}

// ap-ledger: a new Archipelago Ledger, if you lost yours.
auto CommandLedger(D2R::Game::Client*, const D2RL::ConsoleCommandContext*, void*) noexcept -> D2RL::ConsoleCommandResult {
	{
		std::lock_guard lock(gMutex);
		gSave.ledgerGiven = false;
		gLedgerRetryAt = {};
	}
	Reply("A new Archipelago Ledger is on its way.");
	return D2RL::ConsoleCommandResult::Handled;
}

auto CommandCodex(D2R::Game::Client*, const D2RL::ConsoleCommandContext*, void*) noexcept -> D2RL::ConsoleCommandResult {
	{
		std::lock_guard lock(gMutex);
		gCodexDirty = true;
	}
	Reply("Rebuilding the Archipelago Codex.");
	return D2RL::ConsoleCommandResult::Handled;
}

// ap-reconnect: connect again with the saved server and slot.
auto CommandReconnect(D2R::Game::Client*, const D2RL::ConsoleCommandContext*, void*) noexcept -> D2RL::ConsoleCommandResult {
	ap::Config config;
	{
		std::lock_guard lock(gMutex);
		config = gConfig;
		gRepairOffer.Cancel();
	}
	if (config.server.empty() || config.slot.empty()) {
		Reply("No server saved yet: connect once with ap-connect <server[:port]> <slot> [password].");
		return D2RL::ConsoleCommandResult::Failed;
	}
	gClient->Connect(config.server, config.slot, config.password);
	Reply("Connecting to " + config.server + " as " + config.slot + "...");
	return D2RL::ConsoleCommandResult::Handled;
}

// ---- Command and debug panels -------------------------------------------------

auto DebugToolsOn() -> bool {
	std::lock_guard lock(gMutex);
	return gConfig.debugTools;
}

// Opens or closes a panel; userData points at its handle (UI thread).
void __cdecl TogglePanelWork(const D2RL::PluginContext*, void* userData) noexcept {
	if (gPanels == nullptr) {
		return;
	}
	if (const auto result = gPanels->togglePanel(gContext, *static_cast<D2RL::Panels::RegistrationHandle*>(userData)); result != D2RL::Panels::Result::Success) {
		Log("Archipelago: couldn't open a panel (result " + std::to_string(static_cast<uint32_t>(result)) + ").");
	}
}

void __cdecl ClosePanelWork(const D2RL::PluginContext*, void* userData) noexcept {
	if (gPanels != nullptr) {
		gPanels->closePanel(gContext, *static_cast<D2RL::Panels::RegistrationHandle*>(userData));
	}
}

auto CommandPanel(D2R::Game::Client*, const D2RL::ConsoleCommandContext*, void*) noexcept -> D2RL::ConsoleCommandResult {
	if (gPanels == nullptr) {
		gContext->WriteConsoleMessage("ap-panel: this D2RLoader has no panel service.");
		return D2RL::ConsoleCommandResult::Failed;
	}
	TogglePanelWork(gContext, &gPanel);
	return D2RL::ConsoleCommandResult::Handled;
}

auto CommandRepairs(D2R::Game::Client*, const D2RL::ConsoleCommandContext* command, void*) noexcept -> D2RL::ConsoleCommandResult {
	const auto args = SplitArgs(command);
	if (args.size() == 1 && (args[0] == "on" || args[0] == "off")) {
		ap::Config config;
		{
			std::lock_guard lock(gMutex);
			gConfig.repairOnly = args[0] == "on";
			gRepairOffer.Cancel();
			config = gConfig;
		}
		if (!gContext->WriteConfig(ap::FormatConfig(config).c_str())) {
			Reply("ap-repairs: couldn't save the setting; it applies until the plugin reloads.");
		}
		if (gPanels != nullptr && gRepairPanel != D2RL::Panels::InvalidHandle) {
			gPanels->closePanel(gContext, gRepairPanel);
		}
		Reply(config.repairOnly
			? "Repair prototype on. While merchants are locked, talk to a blacksmith and choose Trade/Repair."
			: "Repair prototype off.");
		return D2RL::ConsoleCommandResult::Handled;
	}
	if (args.size() != 1 || args[0] != "all") {
		Reply("Usage: ap-repairs on|off. Use Repair All on the panel after talking to a blacksmith.");
		return D2RL::ConsoleCommandResult::InvalidArguments;
	}
	D2RL::Panels::PanelInfo info { .structSize = D2RL::Panels::PanelInfoSize };
	const bool open = gPanels != nullptr && gRepairPanel != D2RL::Panels::InvalidHandle
		&& gPanels->getPanelInfo(gContext, gRepairPanel, &info) == D2RL::Panels::Result::Success
		&& info.presentationState == D2RL::Panels::PresentationState::Open;
	bool ready = false;
	{
		std::lock_guard lock(gMutex);
		ready = gRepairOffer.Take(open && RepairEligible(), Clock::now());
	}
	if (!ready) {
		Reply("ap-repairs: talk to a blacksmith again; the repair offer expired or isn't active.");
		return D2RL::ConsoleCommandResult::Failed;
	}
	const D2RL::Widgets::UiAction action {
		.structSize = D2RL::Widgets::UiActionSize,
		.target = "VendorPanelMessage",
		.command = "RepairAll",
		.text = "",
	};
	gNativeRepairDispatch = true;
	const auto result = gWidgets->dispatchUiAction(gContext, &action);
	gNativeRepairDispatch = false;
	gPanels->closePanel(gContext, gRepairPanel);
	Log("Archipelago: native RepairAll dispatch result " + std::to_string(static_cast<uint32_t>(result)) + ".");
	// Dispatch success only acknowledges routing, not whether D2R repaired gear.
	Reply(result == D2RL::Widgets::Result::Success
		? "Repair request sent. Check your gear and gold; D2R decides the cost and whether repairs are possible."
		: "D2R couldn't route the repair request. Shopping remains locked.");
	return result == D2RL::Widgets::Result::Success ? D2RL::ConsoleCommandResult::Handled : D2RL::ConsoleCommandResult::Failed;
}

// Key presses arrive during input processing, so the toggle waits for the UI thread.
// userData points at the panel's handle; the debug panel's key does nothing while
// the debug tools are off.
auto __cdecl OnPanelKey(const D2RL::PluginContext*, const D2RL::Input::ActionEvent* event, void* userData) noexcept -> D2RL::Input::ActionResult {
	if (!D2RL::Input::HasActionEventField(event, D2RL::Input::ActionEventRequiredSize) || event->kind != D2RL::Input::ActionEventKind::Pressed
		|| (userData == &gDebugPanel && !DebugToolsOn())) {
		return D2RL::Input::ActionResult::Ignored;
	}
	gThreads->runOnUiThread(gContext, TogglePanelWork, userData);
	return D2RL::Input::ActionResult::Handled;
}

// Items made by ap-debug spawn, waiting for the game thread: a code, and
// whether it's a unique.
std::vector<std::pair<uint32_t, bool>> gSpawnQueue;

// Makes the queued items in the inventory, or at your feet if it's full. They
// aren't marked as rewards, so they count as found.
void __cdecl SpawnWork(const D2RL::PluginContext*, void*) noexcept {
	std::vector<std::pair<uint32_t, bool>> spawns;
	{
		std::lock_guard lock(gMutex);
		spawns.swap(gSpawnQueue);
	}
	D2RL::PlayerHandle player = D2RL::InvalidPlayerHandle;
	if (gInventory->getLocalPlayer(gContext, &player) != D2RL::Inventory::Result::Success) {
		std::lock_guard lock(gMutex);
		gConsoleQueue.push_back("ap-debug: join a game first.");
		return;
	}
	for (const auto& [code, unique] : spawns) {
		auto spec = MakeSpec(code, unique ? D2RL::Items::Quality::Unique : D2RL::Items::Quality::Normal, D2RL::Items::ItemContainer::Inventory);
		spec.itemLevel = unique ? 99 : 1;
		D2RL::ItemHandle item = D2RL::InvalidItemHandle;
		auto result = gItems->createItem(gContext, player, &spec, &item);
		if (result != D2RL::Items::Result::Success) {
			spec.destination.container = D2RL::Items::ItemContainer::Ground;
			result = gItems->createItem(gContext, player, &spec, &item);
		}
		std::string name = "item";
		D2RL::Items::ItemInfo info { .structSize = D2RL::Items::ItemInfoSize };
		if (result == D2RL::Items::Result::Success && gItems->getItemInfo(gContext, item, &info) == D2RL::Items::Result::Success) {
			name = DescribeItem(info);
		}
		std::lock_guard lock(gMutex);
		gConsoleQueue.push_back(result == D2RL::Items::Result::Success
			? "ap-debug: made " + name + "."
			: "ap-debug: couldn't make that item (result " + std::to_string(static_cast<uint32_t>(result)) + ").");
	}
}

// ap-debug on|off turns the debug tools on or off (saved in the config);
// ap-debug opens the debug panel (F10); ap-debug spawn <code> [unique] makes an
// item as if found.
auto CommandDebug(D2R::Game::Client*, const D2RL::ConsoleCommandContext* command, void*) noexcept -> D2RL::ConsoleCommandResult {
	const auto args = SplitArgs(command);
	if (!args.empty() && (args[0] == "on" || args[0] == "off")) {
		ap::Config config;
		{
			std::lock_guard lock(gMutex);
			gConfig.debugTools = args[0] == "on";
			config = gConfig;
		}
		if (!gContext->WriteConfig(ap::FormatConfig(config).c_str())) {
			gContext->WriteConsoleWarning("Couldn't save the debug setting to the config file.");
		}
		if (!config.debugTools && gPanels != nullptr) {
			gPanels->closePanel(gContext, gDebugPanel);
		}
		Reply(config.debugTools ? "Debug tools on: F10 or ap-debug opens the debug panel." : "Debug tools off.");
		return D2RL::ConsoleCommandResult::Handled;
	}
	if (!DebugToolsOn()) {
		Reply("The debug tools are off; turn them on with ap-debug on.");
		return D2RL::ConsoleCommandResult::Failed;
	}
	if (args.empty()) {
		if (gPanels == nullptr) {
			gContext->WriteConsoleMessage("ap-debug: this D2RLoader has no panel service.");
			return D2RL::ConsoleCommandResult::Failed;
		}
		TogglePanelWork(gContext, &gDebugPanel);
		return D2RL::ConsoleCommandResult::Handled;
	}
	if (args[0] == "banner" && args.size() >= 2) {
		static const std::map<std::string, ap::notify::Style> styles {
			{ "important", ap::notify::Style::Important }, { "trap", ap::notify::Style::Trap }, { "check", ap::notify::Style::Check } };
		const auto style = styles.find(args[1]);
		std::string text;
		for (size_t i = style == styles.end() ? 1 : 2; i < args.size(); ++i) {
			text += (text.empty() ? "" : " ") + args[i];
		}
		ap::notify::Show(text.empty() ? "Archipelago banner test" : text, style == styles.end() ? ap::notify::Style::Info : style->second);
		return D2RL::ConsoleCommandResult::Handled;
	}
	if (args[0] == "overlay") {
		gOverlayTest = !gOverlayTest;
		gOverlayTestLogged = 0;
		Reply(gOverlayTest ? "Overlay test on: coloured shapes and a yellow line of text should show." : "Overlay test off.");
		return D2RL::ConsoleCommandResult::Handled;
	}
	if (args[0] == "spawn" && args.size() >= 2 && args[1].size() >= 3 && args[1].size() <= 4) {
		{
			std::lock_guard lock(gMutex);
			gSpawnQueue.emplace_back(ap::loot::CodeFromText(args[1].c_str()), args.size() > 2 && args[2] == "unique");
		}
		if (gThreads->runOnGameThread(gContext, SpawnWork, nullptr) != D2RL::Threads::Result::Success) {
			gContext->WriteConsoleMessage("ap-debug: join a game first.");
			return D2RL::ConsoleCommandResult::Failed;
		}
		return D2RL::ConsoleCommandResult::Handled;
	}
	gContext->WriteConsoleMessage("Usage: ap-debug [on|off], ap-debug spawn <item code> [unique], ap-debug banner [important|trap|check] <text>, ap-debug overlay");
	return D2RL::ConsoleCommandResult::InvalidArguments;
}

// Runs one console command from a panel button.
void RunPanelCommand(std::string_view action) {
	using Handler = D2RL::ConsoleCommandResult (*)(D2R::Game::Client*, const D2RL::ConsoleCommandContext*, void*) noexcept;
	static const std::map<std::string_view, Handler> handlers {
		{ "ap-status", CommandStatus },
		{ "ap-missing", CommandMissing },
		{ "ap-hints", CommandHints },
		{ "ap-ledger", CommandLedger },
		{ "ap-codex", CommandCodex },
		{ "ap-reconnect", CommandReconnect },
		{ "ap-disconnect", CommandDisconnect },
		{ "ap-core", CommandCore },
		{ "ap-relearn", CommandRelearn },
		{ "ap-debug", CommandDebug },
		{ "ap-repairs", CommandRepairs },
	};
	const size_t           space   = action.find(' ');
	const std::string_view name    = action.substr(0, space);
	const std::string_view args    = space == std::string_view::npos ? std::string_view() : action.substr(space + 1);
	const auto             handler = handlers.find(name);
	if (handler == handlers.end()) {
		return;
	}
	const D2RL::ConsoleCommandContext command {
		.contextSize = sizeof(D2RL::ConsoleCommandContext),
		.inputLength = static_cast<uint32_t>(action.size()),
		.nameLength  = static_cast<uint32_t>(name.size()),
		.argsLength  = static_cast<uint32_t>(args.size()),
		.input       = action.data(),
		.name        = name.data(),
		.args        = args.data(),
		.plugin      = gContext,
	};
	handler->second(nullptr, &command, nullptr);
}

// A panel button (UI thread): closes its panel, so the chat with the answer
// shows, and runs the button's commands. Debug buttons only run while the
// debug tools are on.
void RunPanelButton(std::string_view id) {
	if (ap::panel::Find(ap::panel::Repairs(), id) != nullptr) {
		// CommandRepairs requires this modal to still be open and consumes its
		// one-shot offer before dispatch. Unlike other buttons, don't close first.
		RunPanelCommand("ap-repairs all");
		return;
	}
	auto* handle = &gPanel;
	const auto* button = ap::panel::Find(ap::panel::Main(), id);
	if (button == nullptr && DebugToolsOn()) {
		handle = &gDebugPanel;
		button = ap::panel::Find(ap::panel::Debug(), id);
	}
	if (button == nullptr) {
		Log("Archipelago: panel button " + std::string(id) + " isn't on an open panel (debug tools " + (DebugToolsOn() ? "on" : "off") + ").");
		return;
	}
	Log("Archipelago: panel button: " + std::string(button->action));
	gThreads->runOnUiThread(gContext, ClosePanelWork, handle);
	for (const auto part : ap::panel::SplitAction(button->action)) {
		if (!part.starts_with('!')) {
			RunPanelCommand(part);
			continue;
		}
		if (gClient->State() != ap::ConnectionState::Connected) {
			Reply("Not connected to Archipelago: " + gClient->StatusText());
			return;
		}
		gClient->Say(std::string(part));
	}
}

// Registers a panel's layout, the panel and its key. Without them the panel is
// missing, but the console commands still work. False if the panel is missing.
bool RegisterPanel(const D2RL::PluginContext* context, const D2RL::ResourceService* resources, const D2RL::InputService* input,
	const ap::panel::Panel& panel, D2RL::Panels::RegistrationHandle& handle, const char* logicalId, const char* displayName, D2RL::Input::Key key) {
	const std::string layout = ap::panel::Layout(panel);
	const D2RL::Resources::ResourceRegistration resource {
		.structSize = D2RL::Resources::ResourceRegistrationSize,
		.path       = panel.path,
		.bytes      = layout.data(),
		.byteCount  = layout.size(),
	};
	const D2RL::Panels::PanelRegistration registration {
		.structSize = D2RL::Panels::PanelRegistrationSize,
		.flags      = D2RL::Panels::PanelFlags::CloseOnEscape,
		.localId    = panel.localId,
	};
	D2RL::Resources::RegistrationHandle resourceHandle = D2RL::Resources::InvalidHandle;
	if (resources->registerResource(context, &resource, &resourceHandle) != D2RL::Resources::Result::Success
		|| gPanels->registerPanel(context, &registration, &handle) != D2RL::Panels::Result::Success) {
		context->LogWarn(("Archipelago: couldn't register the " + std::string(panel.localId) + ".").c_str());
		return false;
	}
	if (input == nullptr) {
		return true;
	}
	const D2RL::Input::ActionRegistration action {
		.structSize       = D2RL::Input::ActionRegistrationSize,
		.logicalId        = logicalId,
		.displayName      = displayName,
		.category         = "Archipelago",
		.defaultPrimary   = { .key = key },
		.defaultSecondary = { .key = D2RL::Input::Key::None },
		.callback         = OnPanelKey,
		.userData         = &handle,
	};
	D2RL::Input::ActionHandle actionHandle = D2RL::Input::InvalidHandle;
	if (input->registerAction(context, &action, &actionHandle) != D2RL::Input::Result::Success) {
		context->LogWarn(("Archipelago: couldn't register the key for the " + std::string(panel.localId) + "; use its console command.").c_str());
	}
	return true;
}

void RegisterPanels(const D2RL::PluginContext* context) {
	const D2RL::ResourceService* resources = nullptr;
	const D2RL::InputService*    input     = nullptr;
	if (context->QueryService(&resources) != D2RL::ServiceQueryResult::Success || !D2RL::HasResourceServiceField(resources, D2RL::ResourceServiceRequiredSize)
		|| context->QueryService(&gPanels) != D2RL::ServiceQueryResult::Success || !D2RL::HasPanelServiceField(gPanels, D2RL::PanelServiceRequiredSize)) {
		gPanels = nullptr;
		context->LogWarn("Archipelago: no panel service; the command and debug panels are off.");
		return;
	}
	if (context->QueryService(&input) != D2RL::ServiceQueryResult::Success || !D2RL::HasInputServiceField(input, D2RL::InputServiceRequiredSize)) {
		input = nullptr;
		context->LogWarn("Archipelago: no input service; open the panels with ap-panel and ap-debug.");
	}
	const bool main = RegisterPanel(context, resources, input, ap::panel::Main(), gPanel, "command-panel", "Archipelago Commands", D2RL::Input::Key::F9);
	const bool debug = RegisterPanel(context, resources, input, ap::panel::Debug(), gDebugPanel, "debug-panel", "Archipelago Debug Panel", D2RL::Input::Key::F10);
	const bool repairs = RegisterPanel(context, resources, nullptr, ap::panel::Repairs(), gRepairPanel, "repairs", "Repairs", D2RL::Input::Key::None);
	if (!main && !debug && !repairs) {
		gPanels = nullptr;
	}
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
	if (!Query(gGameRules, D2RL::HasGameRuleServiceField, D2RL::GameRuleServiceRequiredSize)) {
		gGameRules = nullptr;
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
	if (context->QueryService(&gInteractions) == D2RL::ServiceQueryResult::Success
		&& D2RL::HasItemInteractionServiceField(gInteractions, D2RL::ItemInteractionServiceRequiredSize)) {
		// Belt clicks too, if this loader supports them, so belt potions can be locked.
		uint32_t mask = D2RL::ItemInteractions::DefaultContainerMask;
		const uint32_t belt = D2RL::Items::ContainerBit(D2RL::Items::ItemContainer::Belt);
		if (D2RL::HasItemInteractionServiceField(gInteractions, D2RL::ItemInteractionServiceSupportedContainerMaskFieldEnd)
			&& (gInteractions->supportedContainerMask & belt) != 0) {
			mask |= belt;
		}
		const D2RL::ItemInteractions::ItemInteractionListener listener {
			.structSize    = D2RL::ItemInteractions::ItemInteractionListenerSize,
			.callback      = OnItemActivate,
			.containerMask = mask,
		};
		D2RL::ItemInteractions::ListenerHandle handle = D2RL::ItemInteractions::InvalidHandle;
		if (gInteractions->registerListener(context, &listener, &handle) != D2RL::ItemInteractions::Result::Success) {
			context->LogWarn("Archipelago: couldn't register the item-use listener; the Town Portal and Potion locks won't work, and quest items used at once may be missed.");
		}
	} else {
		context->LogWarn("Archipelago: no item interaction service; the Town Portal and Potion locks won't work, and quest items used at once may be missed.");
	}
	RegisterPanels(context);
	std::vector<std::string> configWarnings;
	gConfig = ap::ParseConfig(ConfigText(), &configWarnings);
	for (const auto& warning : configWarnings) {
		context->LogWarn(warning.c_str());
	}
	ap::notify::SetSounds((GameDirectory() / ap::sounds::Folder).wstring(), gConfig.bannerSounds, gConfig.bannerVolume);
	gClient = std::make_unique<ap::Client>(Log);

	using Kind = D2RL::Lifecycle::GameplayEventKind;
	for (const auto kind : { Kind::GameJoined, Kind::GameLeft, Kind::LocalPlayerReady, Kind::ActChanged, Kind::LevelChanged, Kind::PlayerLevelChanged,
		     Kind::QuestCompleted, Kind::PlayerResurrected }) {
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
	                   && context->RegisterConsoleCommand("ap-reconnect", CommandReconnect, "Connect again with the saved server and slot.")
	                   && context->RegisterConsoleCommand("ap-disconnect", CommandDisconnect, "Disconnect from the Archipelago server.")
	                   && context->RegisterConsoleCommand("ap-status", CommandStatus, "Show the Archipelago connection and progress.")
	                   && context->RegisterConsoleCommand("ap-say", CommandSay, "Send a chat message to the Archipelago server.")
	                   && context->RegisterConsoleCommand("ap-core", CommandCore, "Run a D2RCore console command: ap-core <command>.")
	                   && context->RegisterConsoleCommand("ap-relearn", CommandRelearn, "Unlock your Archipelago skills again after a respec (ap-relearn confirm if it asks).")
	                   && context->RegisterConsoleCommand("ap-personalize", CommandPersonalize, "Put a name on the item in your hand: ap-personalize [name]; with no name it only logs the item.")
	                   && context->RegisterConsoleCommand("ap-chat", CommandChat, "Show a line in the game chat: ap-chat <text>.")
	                   && context->RegisterConsoleCommand("ap-codex", CommandCodex, "Rebuild the Archipelago Codex charm.")
	                   && context->RegisterConsoleCommand("ap-hints", CommandHints, "List the hints you've found in game.")
	                   && context->RegisterConsoleCommand("ap-ledger", CommandLedger, "Get a new Archipelago Ledger if you lost yours.")
	                   && context->RegisterConsoleCommand("ap-missing", CommandMissing, "List the checks left in this act: ap-missing [all].")
	                   && context->RegisterConsoleCommand("ap-panel", CommandPanel, "Open or close the Archipelago command panel (F9).")
	                   && context->RegisterConsoleCommand("ap-repairs", CommandRepairs, "Experimental repairs with Merchant Lock: ap-repairs on|off; use the NPC repair panel.")
	                   && context->RegisterConsoleCommand("ap-debug", CommandDebug, "Debug tools: ap-debug on|off, ap-debug opens the debug panel (F10), ap-debug spawn <item code> [unique].")
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
	ap::notify::StopSounds();
	gClient.reset();
}
