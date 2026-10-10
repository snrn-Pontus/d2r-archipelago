// Archipelago multiworld client for Diablo II: Resurrected on D2RLoader.
//
// Threads: gameplay events and console commands arrive on the UI thread,
// overlay frames on the render thread, item changes run on the game thread,
// and ap::Client owns a network thread. Everything shared lives under gMutex.

#include "ap_client.h"
#include "config.h"
#include "game_data.h"
#include "ids.h"
#include "save_state.h"

#include <D2RLPlugin/api.h>

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <deque>
#include <filesystem>
#include <memory>
#include <mutex>
#include <random>
#include <set>
#include <string>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

constexpr D2RL::PluginInfo Info {
	.infoSize    = D2RL::PluginInfoSize,
	.abiVersion  = D2RL_PLUGIN_ABI_VERSION,
	.id          = "archipelago",
	.name        = "Archipelago",
	.version     = "0.1.0",
	.author      = "snrn",
	.description = "Archipelago multiworld client: shuffled skills arrive as a Codex charm.",
	.flags       = D2RL::PluginFlags::Shared,
};

// The Codex charm's item seed carries this marker in its top byte, the chunk
// number in the next byte and the number of skills in the low 16 bits.
constexpr uint32_t CodexMarker     = 0xA9000000U;
constexpr uint32_t CodexMarkerMask = 0xFF000000U;
constexpr uint32_t CodexMaxSkills  = 60;
constexpr auto     FeedLifetime    = std::chrono::seconds(12);
constexpr size_t   FeedLines       = 6;

const D2RL::PluginContext*    gContext;
const D2RL::LifecycleService* gLifecycle;
const D2RL::InventoryService* gInventory;
const D2RL::ItemService*      gItems;
const D2RL::ThreadService*    gThreads;
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
int                   gPlayerLevel  = 0;
Clock::time_point     gLastRoomWarning;

void Log(const std::string& text) {
	if (gContext != nullptr) {
		gContext->LogInfo(text.c_str());
	}
}

void Feed(const std::string& text) {
	std::lock_guard lock(gMutex);
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

void AddChecks(const std::vector<int64_t>& locations) {
	std::vector<int64_t> fresh;
	{
		std::lock_guard lock(gMutex);
		for (const int64_t location : locations) {
			const bool added = gBoundSession != 0 ? gSave.checks.insert(location).second : gUnbound.insert(location).second;
			if (added) {
				fresh.push_back(location);
			}
		}
		if (!fresh.empty() && gBoundSession != 0) {
			gSave.Save(gSavePath);
		}
	}
	if (!fresh.empty() && gClient) {
		gClient->SendChecks(fresh);
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
	gClient->SendGoal();
	Feed("Archipelago: goal complete!");
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

// Load the save for a newly connected seed and send what the server is missing.
void BindSession(uint64_t session) {
	const std::string seed = gClient->Seed();
	const std::string slot = gClient->Slot();
	const std::set<int64_t> serverChecked = gClient->Checked();
	std::vector<int64_t> unsent;
	bool goal = false;
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
			if (!serverChecked.contains(check)) {
				unsent.push_back(check);
			}
		}
		goal = gSave.goalReached;
		level = gPlayerLevel;
		gBoundSession = session;
		gCodexDirty = true;
	}
	gClient->SendChecks(unsent);
	if (goal) {
		gClient->SendGoal();
	}
	if (level > 0) {
		CheckLevelMilestones(level);
	}
	Log("Archipelago: bound to seed " + seed + " slot " + slot + ", resent " + std::to_string(unsent.size()) + " checks.");
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

auto CodeFromText(const std::string& text) -> uint32_t {
	char padded[4] = { ' ', ' ', ' ', ' ' };
	std::copy_n(text.begin(), std::min<size_t>(text.size(), 4), padded);
	return D2RL::Items::MakeItemCode(padded[0], padded[1], padded[2], padded[3]);
}

auto FillerCode(int64_t item, std::mt19937& random) -> uint32_t {
	static constexpr const char* PerfectGems[] = { "gpv", "gpb", "gpg", "gpr", "gpw", "gpy", "skz" };
	// Runes are r01 (El) through r33 (Zod); low is El-Thul, mid is Amn-Lem.
	auto rune = [](uint32_t number) {
		char code[5] {};
		std::snprintf(code, sizeof(code), "r%02u", number);
		return CodeFromText(code);
	};
	switch (item - ap::ids::FillerBase) {
		case 0:  return D2RL::Items::MakeItemCode("rvl");
		case 1:  return CodeFromText(PerfectGems[random() % std::size(PerfectGems)]);
		case 2:  return rune(1 + random() % 10);
		case 3:  return rune(11 + random() % 10);
		default: return 0;
	}
}

// Put an item in the inventory, falling back to the cube, stash and ground.
bool GiveItem(D2RL::PlayerHandle player, uint32_t code) {
	for (const auto container : { D2RL::Items::ItemContainer::Inventory, D2RL::Items::ItemContainer::Cube, D2RL::Items::ItemContainer::PersonalStash,
		     D2RL::Items::ItemContainer::Ground }) {
		const auto spec = MakeSpec(code, D2RL::Items::Quality::Normal, container);
		D2RL::ItemHandle item = D2RL::InvalidItemHandle;
		if (gItems->createItem(gContext, player, &spec, &item) == D2RL::Items::Result::Success) {
			return true;
		}
	}
	return false;
}

struct CodexScan {
	uint32_t code;
	std::vector<D2RL::Items::ItemInfo> found;
};

auto __cdecl CollectCodex(const D2RL::PluginContext*, const D2RL::Items::ItemInfo* item, void* userData) noexcept -> D2RL::Inventory::IterationAction {
	auto* scan = static_cast<CodexScan*>(userData);
	if (item != nullptr && item->structSize >= D2RL::Items::ItemInfoRequiredSize && item->code == scan->code
		&& (item->itemSeed & CodexMarkerMask) == CodexMarker) {
		scan->found.push_back(*item);
	}
	return D2RL::Inventory::IterationAction::Continue;
}

auto CodexSeed(uint32_t chunk, uint32_t count) -> uint32_t {
	return CodexMarker | (chunk << 16) | count;
}

// Make sure each 60-skill chunk has exactly one up-to-date Codex charm in the
// inventory. Returns false when an update couldn't be placed.
bool SyncCodex(D2RL::PlayerHandle player, const std::vector<uint32_t>& skills, const ap::Config& config) {
	CodexScan scan { CodeFromText(config.codexItem), {} };
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
		const uint32_t wanted = CodexSeed(chunk, static_cast<uint32_t>(count));
		bool current = false;
		std::vector<D2RL::Items::TransactionInput> stale;
		for (const auto& item : scan.found) {
			if (((item.itemSeed >> 16) & 0xFF) != chunk) {
				continue;
			}
			if (!current && item.itemSeed == wanted && item.container == D2RL::Items::ItemContainer::Inventory) {
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
			properties.push_back(D2RL::Items::MakeExactProperty(config.oskillProperty, 1, static_cast<int32_t>(skills[index])));
		}
		std::vector<D2RL::Items::ItemCreateSpec> outputs;
		if (!current && count > 0) {
			auto spec = MakeSpec(scan.code, config.codexQuality, D2RL::Items::ItemContainer::Inventory);
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
		std::vector<D2RL::ItemHandle> created(outputs.size(), D2RL::InvalidItemHandle);
		const D2RL::Items::Transaction transaction {
			.structSize     = D2RL::Items::TransactionSize,
			.player         = player,
			.inputCount     = static_cast<uint32_t>(stale.size()),
			.outputCount    = static_cast<uint32_t>(outputs.size()),
			.inputs         = stale.data(),
			.outputs        = outputs.data(),
			.outputItems    = created.data(),
			.outputCapacity = static_cast<uint32_t>(created.size()),
		};
		D2RL::Items::TransactionResult result { .structSize = D2RL::Items::TransactionResultSize };
		const auto status = gItems->executeTransaction(gContext, &transaction, &result);
		if (status != D2RL::Items::Result::Success) {
			Log("Archipelago: Codex update failed with result " + std::to_string(static_cast<uint32_t>(status)) + ".");
			ok = false;
		} else if (!outputs.empty()) {
			Log("Archipelago: Codex " + std::to_string(chunk + 1) + " now holds " + std::to_string(count) + " skills.");
		}
	}
	return ok;
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

	std::vector<uint32_t> skills;
	std::set<uint32_t> seen;
	for (const auto& item : items) {
		if (ap::ids::IsSkillItem(item.item) && seen.insert(ap::ids::SkillOf(item.item)).second) {
			skills.push_back(ap::ids::SkillOf(item.item));
		}
	}
	const bool codexOk = SyncCodex(player, skills, config);

	std::mt19937 random(static_cast<uint32_t>(Clock::now().time_since_epoch().count()));
	size_t next = std::min(delivered, items.size());
	for (; next < items.size(); ++next) {
		const uint32_t code = FillerCode(items[next].item, random);
		if (code != 0 && !GiveItem(player, code)) {
			Log("Archipelago: couldn't place a filler item; will retry.");
			break;
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
	} else if (!codexOk && Clock::now() - gLastRoomWarning > std::chrono::seconds(30)) {
		gLastRoomWarning = Clock::now();
		gFeed.push_back({ "Archipelago: make room in your inventory for the Codex charm.", Clock::now() });
	}
}

// ---- Overlay --------------------------------------------------------------

void DrawText(const D2RL::Overlay::Frame* frame, float x, float y, const std::string& text, D2RL::Overlay::Color color) {
	const D2RL::Overlay::TextRequest request {
		.structSize = D2RL::Overlay::TextRequestSize,
		.canvas     = frame->canvas,
		.position   = { x, y },
		.color      = color,
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

	bool work = false;
	std::vector<FeedLine> feed;
	{
		std::lock_guard lock(gMutex);
		work = gInGame && gBoundSession != 0 && (gCodexDirty || gCodexItems != gClient->ItemsGeneration());
		const auto now = Clock::now();
		while (!gFeed.empty() && now - gFeed.front().shown > FeedLifetime) {
			gFeed.pop_front();
		}
		feed.assign(gFeed.begin(), gFeed.end());
	}
	if (work && !gWorkQueued.exchange(true)) {
		if (gThreads->runOnGameThread(gContext, GameWork, nullptr) != D2RL::Threads::Result::Success) {
			gWorkQueued = false;
		}
	}

	const float size = frame->defaultTextSize > 0 ? frame->defaultTextSize : 16.0F;
	std::string status = "AP: " + gClient->StatusText();
	if (gClient->State() == ap::ConnectionState::Connected) {
		status = "AP: " + gClient->Slot() + " - " + std::to_string(gClient->CheckedCount()) + "/" + std::to_string(gClient->LocationCount()) + " checks";
	}
	DrawText(frame, 12.0F, 8.0F, status, { 0.85F, 0.75F, 0.45F, 0.9F });
	float y = 8.0F + size * 1.4F;
	for (const auto& line : feed) {
		DrawText(frame, 12.0F, y, line.text, { 1.0F, 1.0F, 1.0F, 0.95F });
		y += size * 1.25F;
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
			for (const auto& quest : ap::data::Quests) {
				if (quest.flag == event->questRecordId && event->difficulty < 3) {
					AddChecks({ ap::ids::QuestLocation(event->difficulty, quest.flag) });
					const ap::SlotData slots = gClient->Slots();
					if (static_cast<int>(quest.flag) == slots.goalQuest && static_cast<int>(event->difficulty) == slots.goal) {
						ReachGoal();
					}
				}
			}
			break;
		}
		case Kind::LevelChanged: {
			const auto level = event->currentValue;
			for (const auto& area : ap::data::Areas) {
				if (area.id == level) {
					AddChecks({ ap::ids::AreaLocation(area.id) });
				}
			}
			break;
		}
		case Kind::PlayerLevelChanged: {
			{
				std::lock_guard lock(gMutex);
				gPlayerLevel = event->currentValue;
			}
			CheckLevelMilestones(event->currentValue);
			break;
		}
		default: break;
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
	gConfig = ap::ParseConfig(ConfigText());
	gClient = std::make_unique<ap::Client>(Log);

	using Kind = D2RL::Lifecycle::GameplayEventKind;
	for (const auto kind : { Kind::GameJoined, Kind::GameLeft, Kind::LocalPlayerReady, Kind::LevelChanged, Kind::PlayerLevelChanged, Kind::QuestCompleted }) {
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
	                   && context->RegisterConsoleCommand("ap-codex", CommandCodex, "Rebuild the Archipelago Codex charm.");
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
