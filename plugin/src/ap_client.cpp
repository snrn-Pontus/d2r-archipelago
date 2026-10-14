#include "ap_client.h"

#include <nlohmann/json.hpp>

#include <windows.h>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <optional>
#include <random>
#include <type_traits>
#include <utility>

using json = nlohmann::json;

namespace ap {

namespace {

constexpr const char* GameName = "Diablo II Resurrected";
constexpr unsigned short DefaultPort = 38281;
constexpr size_t MaxMessages = 50;
// Others' sends waiting to be taken; a big release can bring hundreds between two frames.
constexpr size_t MaxOtherSends = 500;

auto Widen(const std::string& text) -> std::wstring {
	if (text.empty()) {
		return {};
	}
	const int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
	std::wstring wide(static_cast<size_t>(size), L'\0');
	MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), size);
	return wide;
}

auto RandomUuid() -> std::string {
	std::random_device device;
	std::string uuid;
	for (int i = 0; i < 32; ++i) {
		uuid += "0123456789abcdef"[device() % 16];
	}
	return uuid;
}

struct Target {
	std::wstring host;
	unsigned short port = DefaultPort;
	std::vector<bool> secureAttempts;
};

// Accepts "host", "host:port", "ws://host:port" and "wss://host:port". Without
// a scheme, wss is tried first and plain ws second, like the official clients.
auto ParseServer(std::string server) -> Target {
	Target target;
	if (server.rfind("wss://", 0) == 0) {
		server = server.substr(6);
		target.secureAttempts = { true };
	} else if (server.rfind("ws://", 0) == 0) {
		server = server.substr(5);
		target.secureAttempts = { false };
	} else {
		target.secureAttempts = { true, false };
	}
	if (const auto slash = server.find('/'); slash != std::string::npos) {
		server.resize(slash);
	}
	if (const auto colon = server.rfind(':'); colon != std::string::npos) {
		target.port = static_cast<unsigned short>(std::strtoul(server.c_str() + colon + 1, nullptr, 10));
		server.resize(colon);
	}
	target.host = Widen(server);
	return target;
}

// object[key] when it's there and of T's JSON type, else the fallback. A field of an unexpected
// type must not throw out of a packet (nlohmann's value() does), least of all out of Connected.
template <typename T>
auto Get(const json& object, const char* key, T fallback) -> T {
	if (!object.is_object()) {
		return fallback;
	}
	const auto field = object.find(key);
	if (field == object.end()) {
		return fallback;
	}
	if constexpr (std::is_same_v<T, bool>) {
		return field->is_boolean() ? field->get<bool>() : fallback;
	} else if constexpr (std::is_integral_v<T>) {
		return field->is_number_integer() ? field->get<T>() : fallback;
	} else {
		return field->is_string() ? field->get<std::string>() : fallback;
	}
}

// object[key] when it's a JSON object or array (as asked), else an empty one.
auto Object(const json& object, const char* key) -> json {
	const json field = object.is_object() ? object.value(key, json()) : json();
	return field.is_object() ? field : json::object();
}

auto Array(const json& object, const char* key) -> json {
	const json field = object.is_object() ? object.value(key, json()) : json();
	return field.is_array() ? field : json::array();
}

// The integers in object[key], skipping anything else.
auto Integers(const json& object, const char* key) -> std::vector<int64_t> {
	std::vector<int64_t> values;
	for (const json& value : Array(object, key)) {
		if (value.is_number_integer()) {
			values.push_back(value.get<int64_t>());
		}
	}
	return values;
}

// A whole decimal number in text ("12", "-3"), as PrintJSON parts and slot_info keys carry them.
auto ParseInteger(const std::string& text) -> std::optional<int64_t> {
	int64_t value = 0;
	const char* last = text.data() + text.size();
	const auto [end, error] = std::from_chars(text.data(), last, value);
	return error == std::errc {} && end == last && !text.empty() ? std::optional(value) : std::nullopt;
}

auto ReadSlotData(const json& data) -> SlotData {
	SlotData slots;
	slots.worldVersion = Get(data, "world_version", std::string());
	slots.goal = Get(data, "goal", 0);
	slots.goalAct = Get(data, "goal_act", 5);
	slots.characterClass = Get(data, "character_class", std::string());
	slots.actKeys = Get(data, "act_keys", false);
	slots.skillScaling = Get(data, "skill_scaling", 0);
	slots.skillScalingCap = Get(data, "skill_scaling_cap", 0);
	slots.goalQuest = Get(data, "goal_quest", 40);
	slots.areaChecks = Get(data, "area_checks", true);
	for (const int64_t location : Integers(data, "side_areas")) {
		slots.sideAreas.insert(location);
	}
	slots.levelChecks = Get(data, "level_checks", true);
	slots.levelStep = Get(data, "level_step", 5);
	slots.levelCap = Get(data, "level_cap", 30);
	slots.superUniqueChecks = Get(data, "superunique_checks", false);
	slots.deathLink = Get(data, "death_link", false);
	slots.cowLevelChecks = Get(data, "cow_level_checks", false);
	slots.runeChecks = Get(data, "rune_checks", 0);
	slots.gemChecks = Get(data, "gem_checks", false);
	slots.setChecks = Get(data, "set_checks", false);
	slots.specialItemChecks = Get(data, "special_item_checks", false);
	slots.questItemChecks = Get(data, "quest_item_checks", false);
	slots.shopChecks = Get(data, "shop_checks", 0);
	slots.uberChecks = Get(data, "uber_checks", false);
	slots.killCountChecks = Get(data, "kill_count_checks", false);
	slots.findCountChecks = Get(data, "find_count_checks", false);
	slots.enforceActKeys = Get(data, "enforce_act_keys", false);
	for (const json& hint : Array(data, "hints")) {
		if (hint.is_array() && hint.size() == 3 && std::ranges::all_of(hint, [](const json& part) { return part.is_number_integer(); })) {
			slots.hints.push_back({ hint[0].get<int64_t>(), hint[1].get<int>(), hint[2].get<int64_t>() });
		}
	}
	slots.hintBalance = Get(data, "hint_balance", 0);
	const json sources = Object(data, "hint_sources");
	slots.cainHints = Get(sources, "cain", false);
	slots.pageHintChance = Get(sources, "page_chance", 0);
	slots.prophecyShop = Get(sources, "prophecy_shop", false);
	const json locks = Object(data, "locks");
	slots.locks = {
		.cube = Get(locks, "cube", false),
		.townPortal = Get(locks, "town_portal", false),
		.potions = Get(locks, "potions", false),
		.stash = Get(locks, "stash", false),
		.merchants = Get(locks, "merchants", false),
		.waypoints = Get(locks, "waypoints", false),
	};
	const json traps = Object(data, "traps");
	slots.traps = {
		.waitForTown = Get(traps, "wait_for_town", true),
		.ambushSize = Get(traps, "ambush_size", 6),
		.areaAmbush = Get(traps, "ambush_monsters", std::string("act")) == "area",
		.taxPerLevel = Get(traps, "tax_per_level", 1000),
		.junkSize = Get(traps, "junk_size", 40),
	};
	const json randomizer = Object(data, "randomizer");
	slots.randomizer.seed = Get(randomizer, "seed", 0U);
	slots.randomizer.monsters = static_cast<randomizer::Shuffle>(Get(randomizer, "monster_shuffle", 0));
	slots.randomizer.superUniques = static_cast<randomizer::Shuffle>(Get(randomizer, "superunique_shuffle", 0));
	slots.randomizer.mainPathSuperUniqueCap = Get(randomizer, "main_path_superunique_cap", -1);
	slots.randomizer.mainPathSuperUniqueMinions = Get(randomizer, "main_path_superunique_minions", -1);
	slots.randomizer.mainPathSuperUniqueStrength = Get(randomizer, "main_path_superunique_strength", 100);
	// A number pins Normal's level to the superunique's usual one plus it; missing or null keeps the new monster's.
	const json level = randomizer.value("main_path_superunique_level", json());
	slots.randomizer.mainPathSuperUniqueLevel = level.is_number_integer() ? std::optional<int>(level.get<int>()) : std::nullopt;
	slots.randomizer.superUniqueSpread = Get(randomizer, "superunique_shuffle_spread", 6);
	slots.randomizer.superUniqueMinions = Get(randomizer, "superunique_minions", 100);
	slots.randomizer.shuffleTolerance = Get(randomizer, "monster_shuffle_tolerance", 3);
	slots.randomizer.shuffleCowLevel = Get(randomizer, "shuffle_cow_level", false);
	slots.randomizer.runewords = Get(randomizer, "runeword_shuffle", false);
	slots.randomizer.runeSpread = Get(randomizer, "runeword_rune_spread", 2);
	// Nightmare and Hell follow Normal unless the slot sets them.
	const int xp = Get(randomizer, "xp_multiplier", 100);
	slots.randomizer.xpPercent = { xp, Get(randomizer, "xp_multiplier_nightmare", xp), Get(randomizer, "xp_multiplier_hell", xp) };
	slots.randomizer.itemLevelRequirements = Get(randomizer, "item_level_requirements", true);
	slots.randomizer.itemStatRequirements = Get(randomizer, "item_stat_requirements", true);
	const int density = Get(randomizer, "monster_density", 100);
	slots.randomizer.monsterDensity = { density, Get(randomizer, "monster_density_nightmare", density),
		Get(randomizer, "monster_density_hell", density) };
	slots.randomizer.championPacks = Get(randomizer, "champion_packs", 100);
	slots.randomizer.betterDrops = Get(randomizer, "better_drops", 100);
	slots.randomizer.betterMagic = Get(randomizer, "better_magic", 100);
	for (const json& pair : Array(randomizer, "skill_tree")) {
		if (pair.is_array() && pair.size() == 2 && pair[0].is_number_unsigned() && pair[1].is_number_unsigned()) {
			slots.randomizer.skillTree.emplace_back(pair[0].get<uint32_t>(), pair[1].get<uint32_t>());
		}
	}
	slots.randomizer.treeMode = Get(randomizer, "tree_mode", false);
	const json start = randomizer.value("start_skill", json());
	if (start.is_object()) {
		slots.randomizer.startClass = Get(start, "class", std::string());
		slots.randomizer.startSkill = Get(start, "skill", -1);
	}
	slots.randomizer.superUniqueChecks = slots.superUniqueChecks;
	// Bound first: items() on a temporary would iterate a destroyed object.
	const json loaderSettings = Object(data, "loader_settings");
	for (const auto& [path, value] : loaderSettings.items()) {
		if (value.is_boolean()) {
			slots.loaderSettings.emplace_back(path, value.get<bool>() ? "true" : "false");
		} else if (value.is_number_integer()) {
			slots.loaderSettings.emplace_back(path, std::to_string(value.get<int64_t>()));
		}
	}
	return slots;
}

} // namespace

auto DataPackageGames(const std::set<std::string>& games, const std::map<std::string, std::string>& roomChecksums,
	const std::map<std::string, std::string>& held) -> std::vector<std::string> {
	std::vector<std::string> fetch;
	for (const std::string& game : games) {
		const auto have = held.find(game);
		const auto room = roomChecksums.find(game);
		// A game the room gives no checksum for can't be checked, so what we hold stays.
		if (have == held.end() || (room != roomChecksums.end() && room->second != have->second)) {
			fetch.push_back(game);
		}
	}
	return fetch;
}

auto PrintJsonAudience(const std::string& type, int receiver, int finder, int flags, int me, OtherSends setting) -> PrintAudience {
	if (type == "Tutorial" || type == "TagsChanged") {
		return PrintAudience::Hidden;
	}
	const bool send = type == "ItemSend" || type == "ItemCheat";
	if ((!send && type != "Hint") || receiver == me || finder == me) {
		return PrintAudience::Own;
	}
	const bool shown = send && (setting == OtherSends::All || (setting == OtherSends::Progression && (flags & 1) != 0));
	return shown ? PrintAudience::Others : PrintAudience::Hidden;
}

void OtherSendsFeed::Add(std::string line) {
	// Once a burst runs over, the rest of it is counted rather than trickling in after the count.
	if (skipped_ > 0 || lines_.size() >= Shown) {
		++skipped_;
		return;
	}
	lines_.push_back(std::move(line));
}

void OtherSendsFeed::Skip(size_t count) {
	skipped_ += count;
}

auto OtherSendsFeed::Empty() const -> bool {
	return lines_.empty() && skipped_ == 0;
}

auto OtherSendsFeed::Take(size_t room) -> std::vector<std::string> {
	std::vector<std::string> taken;
	while (taken.size() < room && !lines_.empty()) {
		taken.push_back(std::move(lines_.front()));
		lines_.pop_front();
	}
	if (taken.size() < room && lines_.empty() && skipped_ > 0) {
		taken.push_back("Archipelago: " + std::to_string(skipped_) + (skipped_ == 1 ? " more item" : " more items")
		    + " sent between other players " + (skipped_ == 1 ? "is" : "are") + " in the D2RLoader log.");
		skipped_ = 0;
	}
	return taken;
}

Client::Client(LogFn log) : log_(std::move(log)) {
	thread_ = std::thread([this] { Run(); });
}

Client::~Client() {
	running_ = false;
	{
		std::lock_guard lock(mutex_);
		wantConnection_ = false;
	}
	wake_.notify_all();
	// Shutting down for good: don't wait for the server to answer a close.
	socket_.Abort();
	if (thread_.joinable()) {
		thread_.join();
	}
}

void Client::Connect(const std::string& server, const std::string& slot, const std::string& password) {
	{
		std::lock_guard lock(mutex_);
		server_ = server;
		slot_ = slot;
		password_ = password;
		wantConnection_ = true;
		++targetGeneration_;
		// The old session is over from here on: nothing may bind to it (or its seed) or send into
		// it until the new target's Connected arrives.
		state_ = ConnectionState::Connecting;
		status_ = "Connecting to " + server + "...";
		seed_.clear();
	}
	socket_.Interrupt();
	wake_.notify_all();
}

void Client::Disconnect() {
	{
		std::lock_guard lock(mutex_);
		wantConnection_ = false;
		++targetGeneration_;
		// Not Connected from here on, so nothing is sent into the closed socket and lost.
		SetDisconnected();
	}
	socket_.Interrupt();
	wake_.notify_all();
}

void Client::SetDisconnected() {
	state_ = ConnectionState::Idle;
	status_ = "Disconnected. Use ap-connect <server> <slot> [password] to connect again.";
}

void Client::Run() {
	while (running_) {
		std::string server;
		uint64_t generation = 0;
		{
			std::unique_lock lock(mutex_);
			wake_.wait(lock, [this] { return !running_ || wantConnection_; });
			if (!running_) {
				return;
			}
			server = server_;
			generation = targetGeneration_;
			state_ = ConnectionState::Connecting;
			status_ = "Connecting to " + server + "...";
		}
		const Target target = ParseServer(server);
		std::string lastError, lostBecause;
		for (const bool secure : target.secureAttempts) {
			if (socket_.Connect(target.host, target.port, secure, lastError)) {
				lastError.clear();
				// A Connect or Disconnect during the handshake found no socket to close, so the
				// session would run on the old target (or after ap-disconnect): drop it instead.
				bool current = false;
				{
					std::lock_guard lock(mutex_);
					current = wantConnection_ && generation == targetGeneration_;
				}
				if (current) {
					lostBecause = Session(generation);
				} else {
					socket_.Close();
				}
				break;
			}
			// The target changed during a failed attempt: no plain-ws try at the old one.
			std::lock_guard lock(mutex_);
			if (!wantConnection_ || generation != targetGeneration_) {
				break;
			}
		}
		std::unique_lock lock(mutex_);
		if (state_ == ConnectionState::Refused) {
			continue;
		}
		if (!wantConnection_) {
			// A Connected packet handled while Disconnect ran may have set Connected again.
			SetDisconnected();
			continue;
		}
		if (generation != targetGeneration_) {
			continue;
		}
		const bool wasConnected = state_ == ConnectionState::Connected;
		state_ = ConnectionState::Connecting;
		status_ = (lastError.empty() ? "Connection lost: " + lostBecause : "Could not connect: " + lastError) + ". Retrying in 5 seconds.";
		if (wasConnected) {
			messages_.push_back("Archipelago: connection lost, reconnecting.");
		}
		wake_.wait_for(lock, std::chrono::seconds(5), [&] { return !running_ || generation != targetGeneration_; });
	}
}

auto Client::Session(uint64_t target) -> std::string {
	std::string message, ended;
	while (running_ && socket_.Receive(message, ended)) {
		try {
			Handle(message, target);
		} catch (const std::exception& error) {
			log_(std::string("Archipelago: bad packet: ") + error.what() + " in " + message.substr(0, 300));
		}
	}
	socket_.Close();
	return ended.empty() ? "closed by the plugin" : ended;
}

void Client::Send(const std::string& text) {
	if (!socket_.Send(text)) {
		log_("Archipelago: send failed.");
	}
}

auto Client::IsStale(std::optional<uint64_t> target) const -> bool {
	return target && *target != targetGeneration_;
}

void Client::Notify(const std::string& message, std::optional<uint64_t> target) {
	std::lock_guard lock(mutex_);
	if (IsStale(target)) {
		return;
	}
	messages_.push_back(message);
	while (messages_.size() > MaxMessages) {
		messages_.pop_front();
	}
}

void Client::Handle(const std::string& text, std::optional<uint64_t> target) {
	const json packets = json::parse(text);
	if (!packets.is_array()) {
		log_("Archipelago: bad message (not a packet list): " + text.substr(0, 300));
		return;
	}
	// One bad packet must not take the rest of the message (often the Connected reply's
	// ReceivedItems) down with it.
	for (const json& packet : packets) {
		try {
			HandlePacket(&packet, target);
		} catch (const std::exception& error) {
			log_(std::string("Archipelago: bad packet: ") + error.what() + " in " + packet.dump().substr(0, 300));
		}
	}
}

void Client::HandlePacket(const void* message, std::optional<uint64_t> target) {
	const json& packet = *static_cast<const json*>(message);
	if (target) {
		// From a socket an ap-connect or ap-disconnect has since let go. RoomInfo, Connected and
		// ConnectionRefused check again under their own lock, as a switch can land in between.
		std::lock_guard lock(mutex_);
		if (*target != targetGeneration_) {
			return;
		}
	}
	const std::string cmd = Get(packet, "cmd", std::string());
	if (cmd == "RoomInfo") {
		std::string slot, password;
		std::map<std::string, std::string> checksums;
		const json roomChecksums = Object(packet, "datapackage_checksums");
		for (const auto& [game, checksum] : roomChecksums.items()) {
			if (checksum.is_string()) {
				checksums[game] = checksum.get<std::string>();
			}
		}
		// The multiworld's games ("games" lists them, the server's own "Archipelago" too).
		std::set<std::string> games { GameName };
		for (const json& game : Array(packet, "games")) {
			if (game.is_string()) {
				games.insert(game.get<std::string>());
			}
		}
		std::vector<std::string> fetch;
		{
			std::lock_guard lock(mutex_);
			// From a socket an ap-connect or ap-disconnect has since let go: not the seed we want.
			if (IsStale(target)) {
				return;
			}
			seed_ = Get(packet, "seed_name", std::string());
			slot = slot_;
			password = password_;
			// Names only for this multiworld's games, and only those not already held at the
			// server's checksum, so a reconnect doesn't download (and parse) them all again.
			fetch = DataPackageGames(games, checksums, dataChecksums_);
		}
		// Asked for before Connect, so the names are in before Connected and the first items
		// (whose banners are only shown once).
		if (!fetch.empty()) {
			Send(json::array({ json { { "cmd", "GetDataPackage" }, { "games", fetch } } }).dump());
		}
		const json connect {
			{ "cmd", "Connect" },
			{ "game", GameName },
			{ "name", slot },
			{ "password", password },
			{ "uuid", RandomUuid() },
			{ "version", { { "major", 0 }, { "minor", 6 }, { "build", 4 }, { "class", "Version" } } },
			{ "items_handling", 0b111 },
			{ "tags", json::array() },
			{ "slot_data", true },
		};
		Send(json::array({ connect }).dump());
	} else if (cmd == "Connected") {
		// Read before taking the lock; a field of the wrong type falls back to its default.
		SlotData slots = ReadSlotData(Object(packet, "slot_data"));
		std::lock_guard lock(mutex_);
		// The old target's Connected, read just before an ap-connect: it must not make the client
		// Connected again (the plugin would bind that session with the new slot or an empty seed).
		if (IsStale(target)) {
			return;
		}
		team_ = Get(packet, "team", 0);
		mySlot_ = Get(packet, "slot", 0);
		// A new session: the previous slot's items and players must not carry over. The server
		// follows with ReceivedItems index 0 when this slot has any items (same reply), and sends
		// nothing when it has none, so the list stays empty and unsynced until then.
		items_.clear();
		itemsSynced_ = false;
		++itemsGeneration_;
		playerNames_.clear();
		playerGames_.clear();
		for (const json& player : Array(packet, "players")) {
			if (Get(player, "team", 0) == team_) {
				playerNames_[Get(player, "slot", 0)] = Get(player, "alias", Get(player, "name", std::string()));
			}
		}
		const json slotInfo = Object(packet, "slot_info");
		for (const auto& [slot, info] : slotInfo.items()) {
			if (const auto number = ParseInteger(slot)) {
				playerGames_[static_cast<int>(*number)] = Get(info, "game", std::string());
			}
		}
		checked_.clear();
		for (const int64_t location : Integers(packet, "checked_locations")) {
			checked_.insert(location);
		}
		locations_ = checked_;
		const std::vector<int64_t> missing = Integers(packet, "missing_locations");
		locations_.insert(missing.begin(), missing.end());
		slotData_ = std::move(slots);
		acceptedTargetGeneration_ = target.value_or(targetGeneration_);
		state_ = ConnectionState::Connected;
		status_ = "Connected as " + slot_ + ".";
		++sessionGeneration_;
		messages_.push_back("Archipelago: connected as " + slot_ + ".");
		deaths_.clear();
		// Learn what our unchecked locations hold, for hints about other players' items.
		scouted_.clear();
		if (!missing.empty()) {
			Send(json::array({ json { { "cmd", "LocationScouts" }, { "locations", missing }, { "create_as_hint", 0 } } }).dump());
		}
		if (slotData_.deathLink) {
			// The tag can only be set once slot_data says DeathLink is on.
			Send(R"([{"cmd":"ConnectUpdate","tags":["DeathLink"]}])");
			messages_.push_back("Archipelago: DeathLink is on.");
		}
	} else if (cmd == "ConnectionRefused") {
		std::string reasons;
		for (const json& error : Array(packet, "errors")) {
			if (error.is_string()) {
				reasons += (reasons.empty() ? "" : ", ") + error.get<std::string>();
			}
		}
		std::lock_guard lock(mutex_);
		// An old target's refusal must not cancel the new one.
		if (IsStale(target)) {
			return;
		}
		state_ = ConnectionState::Refused;
		status_ = "Connection refused: " + reasons + ". Check the slot name and password.";
		messages_.push_back("Archipelago: " + status_);
		wantConnection_ = false;
	} else if (cmd == "ReceivedItems") {
		const int64_t index = Get(packet, "index", int64_t { 0 });
		bool resync = false;
		{
			std::lock_guard lock(mutex_);
			if (IsStale(target)) {
				return;
			}
			if (index == 0) {
				items_.clear();
				itemsSynced_ = true;
			}
			if (index < 0 || static_cast<size_t>(index) != items_.size()) {
				resync = true;
			} else {
				for (const json& item : Array(packet, "items")) {
					items_.push_back({ Get(item, "item", int64_t { 0 }), Get(item, "location", int64_t { 0 }), Get(item, "player", 0), Get(item, "flags", 0) });
				}
				++itemsGeneration_;
			}
		}
		if (resync) {
			// We missed a packet; ask for the full list again.
			Send(R"([{"cmd":"Sync"}])");
		}
	} else if (cmd == "RoomUpdate") {
		std::lock_guard lock(mutex_);
		if (IsStale(target)) {
			return;
		}
		for (const int64_t location : Integers(packet, "checked_locations")) {
			checked_.insert(location);
		}
		for (const json& player : Array(packet, "players")) {
			if (Get(player, "team", 0) == team_) {
				playerNames_[Get(player, "slot", 0)] = Get(player, "alias", Get(player, "name", std::string()));
			}
		}
	} else if (cmd == "LocationInfo") {
		std::lock_guard lock(mutex_);
		if (IsStale(target)) {
			return;
		}
		for (const json& item : Array(packet, "locations")) {
			const int64_t location = Get(item, "location", int64_t { 0 });
			scouted_[location] = { Get(item, "item", int64_t { 0 }), location, Get(item, "player", 0), Get(item, "flags", 0) };
		}
	} else if (cmd == "Bounced") {
		const json tags = Array(packet, "tags");
		if (std::ranges::find(tags, json("DeathLink")) == tags.end()) {
			return;
		}
		const json data = Object(packet, "data");
		const std::string source = Get(data, "source", std::string());
		const std::string cause = Get(data, "cause", std::string());
		std::lock_guard lock(mutex_);
		// The server bounces our own deaths back to us too. An abandoned session's death must not
		// kill the player after ap-connect or ap-disconnect.
		if (IsStale(target) || !slotData_.deathLink || source == slot_) {
			return;
		}
		deaths_.push_back(!cause.empty() ? cause : (source.empty() ? "Someone" : source) + " died.");
	} else if (cmd == "DataPackage") {
		// Parsed outside the lock (a big multiworld's package takes a while), then swapped in.
		struct GameNames {
			std::unordered_map<int64_t, std::string> items, locations;
			std::string checksum;
		};
		std::map<std::string, GameNames> parsed;
		// Bind each sub-object before calling items(): Object() returns a
		// temporary, and items() on it would iterate a destroyed object.
		const json games = Object(Object(packet, "data"), "games");
		for (const auto& [game, data] : games.items()) {
			GameNames& names = parsed[game];
			const json itemIds = Object(data, "item_name_to_id");
			for (const auto& [name, id] : itemIds.items()) {
				if (id.is_number_integer()) {
					names.items[id.get<int64_t>()] = name;
				}
			}
			const json locationIds = Object(data, "location_name_to_id");
			for (const auto& [name, id] : locationIds.items()) {
				if (id.is_number_integer()) {
					names.locations[id.get<int64_t>()] = name;
				}
			}
			names.checksum = Get(data, "checksum", std::string());
		}
		std::lock_guard lock(mutex_);
		for (auto& [game, names] : parsed) {
			itemNames_[game] = std::move(names.items);
			locationNames_[game] = std::move(names.locations);
			dataChecksums_[game] = std::move(names.checksum);
		}
	} else if (cmd == "PrintJSON") {
		const std::string type = Get(packet, "type", std::string());
		const json item = Object(packet, "item");
		int me = 0;
		OtherSends setting = OtherSends::Progression;
		{
			std::lock_guard lock(mutex_);
			me = mySlot_;
			setting = otherSends_;
		}
		const PrintAudience audience
		    = PrintJsonAudience(type, Get(packet, "receiving", -1), Get(item, "player", -1), Get(item, "flags", 0), me, setting);
		if (audience == PrintAudience::Hidden) {
			return;
		}
		const std::string line = Render(&packet);
		if (line.empty()) {
			return;
		}
		if (audience == PrintAudience::Own) {
			Notify(line, target);
			return;
		}
		// Others' sends have their own queue, so a release can't push our messages out of ours.
		// One the queue lets go is still logged.
		std::string dropped;
		{
			std::lock_guard lock(mutex_);
			if (IsStale(target)) {
				return;
			}
			otherSendLines_.push_back(line);
			if (otherSendLines_.size() > MaxOtherSends) {
				dropped = std::move(otherSendLines_.front());
				otherSendLines_.pop_front();
				++otherSendsDropped_;
			}
		}
		if (!dropped.empty()) {
			log_("Archipelago: not shown: " + dropped);
		}
	}
}

auto Client::Render(const void* printJson) const -> std::string {
	const json& packet = *static_cast<const json*>(printJson);
	std::string line;
	for (const json& part : Array(packet, "data")) {
		const std::string type = Get(part, "type", std::string("text"));
		const std::string text = Get(part, "text", std::string());
		// Ids arrive as text; one that isn't a number is shown as it came.
		const std::optional<int64_t> id = ParseInteger(text);
		if (type == "player_id" && id) {
			line += PlayerName(static_cast<int>(*id));
		} else if (type == "item_id" && id) {
			line += ItemName(*id, Get(part, "player", 0));
		} else if (type == "location_id" && id) {
			line += LocationName(*id, Get(part, "player", 0));
		} else {
			line += text;
		}
	}
	return line;
}

auto Client::PlayerName(int slot) const -> std::string {
	std::lock_guard lock(mutex_);
	const auto name = playerNames_.find(slot);
	return name != playerNames_.end() ? name->second : "player " + std::to_string(slot);
}

auto Client::GameOf(int slot) const -> std::string {
	std::lock_guard lock(mutex_);
	const auto game = playerGames_.find(slot);
	return game != playerGames_.end() ? game->second : std::string();
}

auto Client::LocationName(int64_t location, int player) const -> std::string {
	const std::string game = GameOf(player);
	std::lock_guard lock(mutex_);
	const auto names = locationNames_.find(game);
	if (names != locationNames_.end()) {
		const auto name = names->second.find(location);
		if (name != names->second.end()) {
			return name->second;
		}
	}
	return "location " + std::to_string(location);
}

auto Client::ItemName(int64_t item, int player) const -> std::string {
	const std::string game = GameOf(player);
	std::lock_guard lock(mutex_);
	const auto names = itemNames_.find(game.empty() ? GameName : game);
	if (names != itemNames_.end()) {
		const auto name = names->second.find(item);
		if (name != names->second.end()) {
			return name->second;
		}
	}
	return "item " + std::to_string(item);
}

void Client::SendChecks(const std::vector<int64_t>& locations) {
	if (locations.empty() || State() != ConnectionState::Connected) {
		return;
	}
	Send(json::array({ json { { "cmd", "LocationChecks" }, { "locations", locations } } }).dump());
}

void Client::SendGoal() {
	if (State() == ConnectionState::Connected) {
		Send(R"([{"cmd":"StatusUpdate","status":30}])");
	}
}

void Client::CreateHint(int64_t location, int player) {
	if (State() == ConnectionState::Connected) {
		Send(json::array({ json { { "cmd", "CreateHints" }, { "locations", { location } }, { "player", player } } }).dump());
	}
}

void Client::HintOwnLocation(int64_t location) {
	if (State() == ConnectionState::Connected) {
		// 2: broadcast only if it's a new hint.
		Send(json::array({ json { { "cmd", "LocationScouts" }, { "locations", { location } }, { "create_as_hint", 2 } } }).dump());
	}
}

auto HintPackets(const std::vector<HintEntry>& own, const std::vector<int64_t>& others) -> std::string {
	json packets = json::array();
	for (const HintEntry& hint : own) {
		packets.push_back({ { "cmd", "CreateHints" }, { "locations", { hint.location } }, { "player", hint.player } });
	}
	if (!others.empty()) {
		// 2: broadcast only if it's a new hint.
		packets.push_back({ { "cmd", "LocationScouts" }, { "locations", others }, { "create_as_hint", 2 } });
	}
	return packets.empty() ? std::string() : packets.dump();
}

void Client::ResendHints(const std::vector<HintEntry>& own, const std::vector<int64_t>& others) {
	if (const std::string packets = HintPackets(own, others); !packets.empty() && State() == ConnectionState::Connected) {
		Send(packets);
	}
}

auto Client::MySlot() const -> int {
	std::lock_guard lock(mutex_);
	return mySlot_;
}

auto Client::Scouted() const -> std::map<int64_t, NetworkItem> {
	std::lock_guard lock(mutex_);
	return scouted_;
}

void Client::SendDeath(const std::string& cause) {
	std::string slot;
	{
		std::lock_guard lock(mutex_);
		if (state_ != ConnectionState::Connected || !slotData_.deathLink) {
			return;
		}
		slot = slot_;
	}
	const double now = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
	const json bounce {
		{ "cmd", "Bounce" },
		{ "tags", { "DeathLink" } },
		{ "data", { { "time", now }, { "source", slot }, { "cause", cause } } },
	};
	Send(json::array({ bounce }).dump());
}

void Client::Say(const std::string& text) {
	if (State() == ConnectionState::Connected) {
		Send(json::array({ json { { "cmd", "Say" }, { "text", text } } }).dump());
	}
}

auto Client::State() const -> ConnectionState {
	std::lock_guard lock(mutex_);
	return state_;
}

auto Client::StatusText() const -> std::string {
	std::lock_guard lock(mutex_);
	return status_;
}

auto Client::HasCurrentSession() const -> bool {
	std::lock_guard lock(mutex_);
	return state_ == ConnectionState::Connected && acceptedTargetGeneration_ == targetGeneration_;
}

auto Client::TargetGeneration() const -> uint64_t {
	std::lock_guard lock(mutex_);
	return targetGeneration_;
}

auto Client::SessionGeneration() const -> uint64_t {
	std::lock_guard lock(mutex_);
	return sessionGeneration_;
}

auto Client::ConnectedSession() const -> uint64_t {
	std::lock_guard lock(mutex_);
	return state_ == ConnectionState::Connected ? sessionGeneration_ : 0;
}

auto Client::Seed() const -> std::string {
	std::lock_guard lock(mutex_);
	return seed_;
}

auto Client::Slot() const -> std::string {
	std::lock_guard lock(mutex_);
	return slot_;
}

auto Client::Slots() const -> SlotData {
	std::lock_guard lock(mutex_);
	return slotData_;
}

auto Client::Checked() const -> std::set<int64_t> {
	std::lock_guard lock(mutex_);
	return checked_;
}

auto Client::CheckedCount() const -> size_t {
	std::lock_guard lock(mutex_);
	return checked_.size();
}

auto Client::LocationCount() const -> size_t {
	std::lock_guard lock(mutex_);
	return locations_.size();
}

auto Client::Missing() const -> std::vector<int64_t> {
	std::lock_guard lock(mutex_);
	std::vector<int64_t> missing;
	for (const int64_t location : locations_) {
		if (!checked_.contains(location)) {
			missing.push_back(location);
		}
	}
	return missing;
}

auto Client::ItemsSynced() const -> bool {
	std::lock_guard lock(mutex_);
	return itemsSynced_;
}

auto Client::Snapshot() const -> ItemSnapshot {
	std::lock_guard lock(mutex_);
	return { items_, slotData_, itemsGeneration_, sessionGeneration_, itemsSynced_ };
}

auto Client::ItemsGeneration() const -> uint64_t {
	std::lock_guard lock(mutex_);
	return itemsGeneration_;
}

auto Client::Items() const -> std::vector<NetworkItem> {
	std::lock_guard lock(mutex_);
	return items_;
}

auto Client::TakeDeaths() -> std::vector<std::string> {
	std::lock_guard lock(mutex_);
	return std::exchange(deaths_, {});
}

auto Client::TakeMessages() -> std::vector<std::string> {
	std::lock_guard lock(mutex_);
	std::vector<std::string> taken(messages_.begin(), messages_.end());
	messages_.clear();
	return taken;
}

void Client::SetOtherSends(OtherSends setting) {
	std::lock_guard lock(mutex_);
	otherSends_ = setting;
}

auto Client::TakeOtherSends() -> OtherSendLines {
	std::lock_guard lock(mutex_);
	OtherSendLines taken { { otherSendLines_.begin(), otherSendLines_.end() }, std::exchange(otherSendsDropped_, 0) };
	otherSendLines_.clear();
	return taken;
}

} // namespace ap
