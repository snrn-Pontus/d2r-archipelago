#pragma once

#include "randomizer.h"
#include "websocket.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace ap {

struct NetworkItem {
	int64_t item     = 0;
	int64_t location = 0;
	int     player   = 0;
	int     flags    = 0;
};

enum class ConnectionState { Idle, Connecting, Connected, Refused };

// One of this player's important items, for hints found in game.
struct HintEntry {
	int64_t location = 0;
	int     player   = 0; // whose world it's in
	int64_t item     = 0;
};

struct SlotData {
	// The apworld version that generated the seed ("0.2.0"), or empty.
	std::string worldVersion;
	int  goal        = 0;
	int  goalAct     = 5;
	// The class the seed was made for ("Barbarian"), or empty for any.
	std::string characterClass;
	// Checks in an act are held until its key arrives.
	bool actKeys     = false;
	// Codex skills gain +1 per this many character levels (0 = off).
	int  skillScaling = 0;
	// The most levels scaling adds (0 = no cap).
	int  skillScalingCap = 0;
	int  goalQuest   = 40;
	bool areaChecks  = true;
	// Side Area Checks: the location ids of the side areas this seed picked.
	std::set<int64_t> sideAreas;
	bool levelChecks = true;
	int  levelStep   = 5;
	int  levelCap    = 30;
	// Field superunique kills are checks.
	bool superUniqueChecks = false;
	bool cowLevelChecks = false;
	// Runes El..this one are checks (0 = none).
	int  runeChecks = 0;
	bool gemChecks = false;
	bool setChecks = false;
	bool specialItemChecks = false;
	bool questItemChecks = false;
	int  shopChecks = 0;
	bool uberChecks = false;
	bool killCountChecks = false;
	bool findCountChecks = false;
	// Walking into a locked act warps you back to town.
	bool enforceActKeys = false;
	// Hints found in game: where this player's items are (in order), and the
	// share (percent) that point at other players' items in this world instead.
	std::vector<HintEntry> hints;
	int  hintBalance = 0;
	bool cainHints = false;
	int  pageHintChance = 0;
	bool prophecyShop = false;
	// What's blocked until its lock item arrives.
	struct Locks {
		bool cube = false, townPortal = false, potions = false, stash = false, merchants = false, waypoints = false;
	} locks;
	// Deaths are shared with the other DeathLink players.
	bool deathLink   = false;
	randomizer::Settings randomizer;
};

// Archipelago protocol client. Runs its own network thread; every public
// method is thread-safe.
class Client {
public:
	using LogFn = std::function<void(const std::string&)>;

	explicit Client(LogFn log);
	~Client();

	void Connect(const std::string& server, const std::string& slot, const std::string& password);
	void Disconnect();

	void SendChecks(const std::vector<int64_t>& locations);
	void SendGoal();
	void Say(const std::string& text);
	// Registers a hint for a location holding one of our items (any world, or
	// our own), without spending hint points.
	void CreateHint(int64_t location, int player);
	// Announces our own location's item as a hint to whoever it belongs to.
	void HintOwnLocation(int64_t location);
	// Tells the other DeathLink players that we died.
	void SendDeath(const std::string& cause);

	auto State() const -> ConnectionState;
	auto StatusText() const -> std::string;
	// Bumped on every successful Connected packet.
	auto SessionGeneration() const -> uint64_t;
	auto Seed() const -> std::string;
	auto Slot() const -> std::string;
	auto Slots() const -> SlotData;
	auto Checked() const -> std::set<int64_t>;
	auto CheckedCount() const -> size_t;
	auto LocationCount() const -> size_t;
	// Our locations the server hasn't seen checked, in id order.
	auto Missing() const -> std::vector<int64_t>;
	// Bumped whenever the received-item list changes.
	auto ItemsGeneration() const -> uint64_t;
	auto Items() const -> std::vector<NetworkItem>;
	auto TakeMessages() -> std::vector<std::string>;
	// Deaths from other DeathLink players since the last call, as messages.
	auto TakeDeaths() -> std::vector<std::string>;
	auto ItemName(int64_t item, int player) const -> std::string;
	auto PlayerName(int slot) const -> std::string;
	auto LocationName(int64_t location, int player) const -> std::string;
	auto MySlot() const -> int;
	// What our own unchecked locations hold, scouted when we connect.
	auto Scouted() const -> std::map<int64_t, NetworkItem>;

	// Processes one server message. The network thread calls it; tests feed it
	// packets directly.
	void Handle(const std::string& text);

private:
	void Run();
	void Session();
	void Send(const std::string& text);
	void Notify(const std::string& message);
	auto Render(const void* printJson) const -> std::string;
	auto GameOf(int slot) const -> std::string;

	LogFn log_;
	WebSocket socket_;
	std::thread thread_;
	std::atomic<bool> running_ { true };

	mutable std::mutex mutex_;
	std::condition_variable wake_;
	std::string server_, slot_, password_;
	uint64_t targetGeneration_ = 0;
	bool wantConnection_ = false;

	ConnectionState state_ = ConnectionState::Idle;
	std::string status_ = "Not connected. Use ap-connect <server> <slot> [password].";
	uint64_t sessionGeneration_ = 0;
	std::string seed_;
	int team_ = 0, mySlot_ = 0;
	SlotData slotData_;
	std::set<int64_t> checked_;
	// Every location in our world, checked or not.
	std::set<int64_t> locations_;
	std::vector<NetworkItem> items_;
	uint64_t itemsGeneration_ = 0;
	std::deque<std::string> messages_;
	std::vector<std::string> deaths_;
	std::map<int64_t, NetworkItem> scouted_;
	std::unordered_map<int, std::string> playerNames_;
	std::unordered_map<int, std::string> playerGames_;
	std::unordered_map<std::string, std::unordered_map<int64_t, std::string>> itemNames_;
	std::unordered_map<std::string, std::unordered_map<int64_t, std::string>> locationNames_;
};

} // namespace ap
