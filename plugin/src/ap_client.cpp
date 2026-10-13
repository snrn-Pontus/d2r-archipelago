#include "ap_client.h"

#include <nlohmann/json.hpp>

#include <windows.h>

#include <algorithm>
#include <chrono>
#include <random>
#include <utility>

using json = nlohmann::json;

namespace ap {

namespace {

constexpr const char* GameName = "Diablo II Resurrected";
constexpr unsigned short DefaultPort = 38281;
constexpr size_t MaxMessages = 50;

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

} // namespace

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
	socket_.Close();
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
	}
	socket_.Close();
	wake_.notify_all();
}

void Client::Disconnect() {
	{
		std::lock_guard lock(mutex_);
		wantConnection_ = false;
		++targetGeneration_;
	}
	socket_.Close();
	wake_.notify_all();
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
		std::string lastError;
		for (const bool secure : target.secureAttempts) {
			if (socket_.Connect(target.host, target.port, secure, lastError)) {
				lastError.clear();
				Session();
				break;
			}
		}
		std::unique_lock lock(mutex_);
		if (state_ == ConnectionState::Refused || !wantConnection_ || generation != targetGeneration_) {
			continue;
		}
		const bool wasConnected = state_ == ConnectionState::Connected;
		state_ = ConnectionState::Connecting;
		status_ = (lastError.empty() ? std::string("Connection lost") : "Could not connect: " + lastError) + ". Retrying in 5 seconds.";
		if (wasConnected) {
			messages_.push_back("Archipelago: connection lost, reconnecting.");
		}
		wake_.wait_for(lock, std::chrono::seconds(5), [&] { return !running_ || generation != targetGeneration_; });
	}
}

void Client::Session() {
	std::string message;
	while (running_ && socket_.Receive(message)) {
		try {
			Handle(message);
		} catch (const std::exception& error) {
			log_(std::string("Archipelago: bad packet: ") + error.what() + " in " + message.substr(0, 300));
		}
	}
	socket_.Close();
}

void Client::Send(const std::string& text) {
	if (!socket_.Send(text)) {
		log_("Archipelago: send failed.");
	}
}

void Client::Notify(const std::string& message) {
	std::lock_guard lock(mutex_);
	messages_.push_back(message);
	while (messages_.size() > MaxMessages) {
		messages_.pop_front();
	}
}

void Client::Handle(const std::string& text) {
	const json packets = json::parse(text);
	for (const json& packet : packets) {
		const std::string cmd = packet.value("cmd", "");
		if (cmd == "RoomInfo") {
			std::string slot, password;
			{
				std::lock_guard lock(mutex_);
				seed_ = packet.value("seed_name", "");
				slot = slot_;
				password = password_;
			}
			json games = packet.value("games", json::array());
			Send(json::array({ json { { "cmd", "GetDataPackage" }, { "games", games } } }).dump());
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
			std::lock_guard lock(mutex_);
			team_ = packet.value("team", 0);
			mySlot_ = packet.value("slot", 0);
			for (const json& player : packet.value("players", json::array())) {
				if (player.value("team", 0) == team_) {
					playerNames_[player.value("slot", 0)] = player.value("alias", player.value("name", ""));
				}
			}
			const json slotInfo = packet.value("slot_info", json::object());
			for (const auto& [slot, info] : slotInfo.items()) {
				playerGames_[std::stoi(slot)] = info.value("game", "");
			}
			checked_.clear();
			for (const json& location : packet.value("checked_locations", json::array())) {
				checked_.insert(location.get<int64_t>());
			}
			locations_ = checked_;
			for (const json& location : packet.value("missing_locations", json::array())) {
				locations_.insert(location.get<int64_t>());
			}
			const json data = packet.value("slot_data", json::object());
			slotData_.worldVersion = data.value("world_version", "");
			slotData_.goal = data.value("goal", 0);
			slotData_.goalAct = data.value("goal_act", 5);
			slotData_.characterClass = data.value("character_class", "");
			slotData_.actKeys = data.value("act_keys", false);
			slotData_.skillScaling = data.value("skill_scaling", 0);
			slotData_.skillScalingCap = data.value("skill_scaling_cap", 0);
			slotData_.goalQuest = data.value("goal_quest", 40);
			slotData_.areaChecks = data.value("area_checks", true);
			slotData_.sideAreas.clear();
			for (const json& location : data.value("side_areas", json::array())) {
				slotData_.sideAreas.insert(location.get<int64_t>());
			}
			slotData_.levelChecks = data.value("level_checks", true);
			slotData_.levelStep = data.value("level_step", 5);
			slotData_.levelCap = data.value("level_cap", 30);
			slotData_.superUniqueChecks = data.value("superunique_checks", false);
			slotData_.deathLink = data.value("death_link", false);
			slotData_.cowLevelChecks = data.value("cow_level_checks", false);
			slotData_.runeChecks = data.value("rune_checks", 0);
			slotData_.gemChecks = data.value("gem_checks", false);
			slotData_.setChecks = data.value("set_checks", false);
			slotData_.specialItemChecks = data.value("special_item_checks", false);
			slotData_.questItemChecks = data.value("quest_item_checks", false);
			slotData_.shopChecks = data.value("shop_checks", 0);
			slotData_.uberChecks = data.value("uber_checks", false);
			slotData_.killCountChecks = data.value("kill_count_checks", false);
			slotData_.findCountChecks = data.value("find_count_checks", false);
			slotData_.enforceActKeys = data.value("enforce_act_keys", false);
			slotData_.hints.clear();
			for (const json& hint : data.value("hints", json::array())) {
				if (hint.is_array() && hint.size() == 3) {
					slotData_.hints.push_back({ hint[0].get<int64_t>(), hint[1].get<int>(), hint[2].get<int64_t>() });
				}
			}
			slotData_.hintBalance = data.value("hint_balance", 0);
			const json sources = data.value("hint_sources", json::object());
			slotData_.cainHints = sources.value("cain", false);
			slotData_.pageHintChance = sources.value("page_chance", 0);
			slotData_.prophecyShop = sources.value("prophecy_shop", false);
			const json locks = data.value("locks", json::object());
			slotData_.locks = {
				.cube = locks.value("cube", false),
				.townPortal = locks.value("town_portal", false),
				.potions = locks.value("potions", false),
				.stash = locks.value("stash", false),
				.merchants = locks.value("merchants", false),
				.waypoints = locks.value("waypoints", false),
			};
			const json randomizer = data.value("randomizer", json::object());
			slotData_.randomizer.seed = randomizer.value("seed", 0U);
			slotData_.randomizer.monsters = static_cast<randomizer::Shuffle>(randomizer.value("monster_shuffle", 0));
			slotData_.randomizer.superUniques = static_cast<randomizer::Shuffle>(randomizer.value("superunique_shuffle", 0));
			slotData_.randomizer.runewords = randomizer.value("runeword_shuffle", false);
			slotData_.randomizer.xpPercent = randomizer.value("xp_multiplier", 100);
			slotData_.randomizer.itemLevelRequirements = randomizer.value("item_level_requirements", true);
			slotData_.randomizer.itemStatRequirements = randomizer.value("item_stat_requirements", true);
			slotData_.randomizer.monsterDensity = randomizer.value("monster_density", 100);
			slotData_.randomizer.championPacks = randomizer.value("champion_packs", 100);
			slotData_.randomizer.betterDrops = randomizer.value("better_drops", 100);
			slotData_.randomizer.skillTree.clear();
			for (const json& pair : randomizer.value("skill_tree", json::array())) {
				if (pair.is_array() && pair.size() == 2) {
					slotData_.randomizer.skillTree.emplace_back(pair[0].get<uint32_t>(), pair[1].get<uint32_t>());
				}
			}
			slotData_.randomizer.treeMode = randomizer.value("tree_mode", false);
			slotData_.randomizer.superUniqueChecks = slotData_.superUniqueChecks;
			state_ = ConnectionState::Connected;
			status_ = "Connected as " + slot_ + ".";
			++sessionGeneration_;
			messages_.push_back("Archipelago: connected as " + slot_ + ".");
			deaths_.clear();
			// Learn what our unchecked locations hold, for hints about other players' items.
			scouted_.clear();
			const json missing = packet.value("missing_locations", json::array());
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
			for (const json& error : packet.value("errors", json::array())) {
				reasons += (reasons.empty() ? "" : ", ") + error.get<std::string>();
			}
			std::lock_guard lock(mutex_);
			state_ = ConnectionState::Refused;
			status_ = "Connection refused: " + reasons + ". Check the slot name and password.";
			messages_.push_back("Archipelago: " + status_);
			wantConnection_ = false;
		} else if (cmd == "ReceivedItems") {
			const size_t index = packet.value("index", 0);
			bool resync = false;
			{
				std::lock_guard lock(mutex_);
				if (index == 0) {
					items_.clear();
				}
				if (index != items_.size()) {
					resync = true;
				} else {
					for (const json& item : packet.value("items", json::array())) {
						items_.push_back({ item.value("item", int64_t { 0 }), item.value("location", int64_t { 0 }), item.value("player", 0), item.value("flags", 0) });
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
			for (const json& location : packet.value("checked_locations", json::array())) {
				checked_.insert(location.get<int64_t>());
			}
			for (const json& player : packet.value("players", json::array())) {
				if (player.value("team", 0) == team_) {
					playerNames_[player.value("slot", 0)] = player.value("alias", player.value("name", ""));
				}
			}
		} else if (cmd == "LocationInfo") {
			std::lock_guard lock(mutex_);
			for (const json& item : packet.value("locations", json::array())) {
				const int64_t location = item.value("location", int64_t { 0 });
				scouted_[location] = { item.value("item", int64_t { 0 }), location, item.value("player", 0), item.value("flags", 0) };
			}
		} else if (cmd == "Bounced") {
			const json tags = packet.value("tags", json::array());
			if (std::ranges::find(tags, json("DeathLink")) == tags.end()) {
				continue;
			}
			const json data = packet.value("data", json::object());
			const std::string source = data.value("source", "");
			const std::string cause = data.value("cause", "");
			std::lock_guard lock(mutex_);
			// The server bounces our own deaths back to us too.
			if (!slotData_.deathLink || source == slot_) {
				continue;
			}
			deaths_.push_back(!cause.empty() ? cause : (source.empty() ? "Someone" : source) + " died.");
		} else if (cmd == "DataPackage") {
			std::lock_guard lock(mutex_);
			// Bind each sub-object before calling items(): value() returns a
			// temporary, and items() on it would iterate a destroyed object.
			const json games = packet.at("data").at("games");
			for (const auto& [game, data] : games.items()) {
				const json itemIds = data.value("item_name_to_id", json::object());
				for (const auto& [name, id] : itemIds.items()) {
					itemNames_[game][id.get<int64_t>()] = name;
				}
				const json locationIds = data.value("location_name_to_id", json::object());
				for (const auto& [name, id] : locationIds.items()) {
					locationNames_[game][id.get<int64_t>()] = name;
				}
			}
		} else if (cmd == "PrintJSON") {
			const std::string type = packet.value("type", "");
			if (type == "Tutorial" || type == "TagsChanged") {
				continue;
			}
			if (type == "ItemSend" || type == "ItemCheat" || type == "Hint") {
				int me = 0;
				{
					std::lock_guard lock(mutex_);
					me = mySlot_;
				}
				const int receiver = packet.value("receiving", -1);
				const int finder = packet.contains("item") ? packet["item"].value("player", -1) : -1;
				if (receiver != me && finder != me) {
					continue;
				}
			}
			const std::string line = Render(&packet);
			if (!line.empty()) {
				Notify(line);
			}
		}
	}
}

auto Client::Render(const void* printJson) const -> std::string {
	const json& packet = *static_cast<const json*>(printJson);
	std::string line;
	for (const json& part : packet.value("data", json::array())) {
		const std::string type = part.value("type", "text");
		const std::string text = part.value("text", "");
		if (type == "player_id") {
			line += PlayerName(std::stoi(text));
		} else if (type == "item_id") {
			line += ItemName(std::stoll(text), part.value("player", 0));
		} else if (type == "location_id") {
			line += LocationName(std::stoll(text), part.value("player", 0));
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

auto Client::SessionGeneration() const -> uint64_t {
	std::lock_guard lock(mutex_);
	return sessionGeneration_;
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

} // namespace ap
