#pragma once

#include "act_lock.h"
#include "config.h"
#include "potion_lock.h"
#include "randomizer.h"
#include "websocket.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
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
	// Codex levels per copy of a skill and per Codex Empowerment, and the most a skill gets.
	int  levelsPerCopy = 1;
	int  levelsPerEmpowerment = 1;
	int  codexLevelCap = 60;
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
	// Walking into a locked act warps you back to town, or only says so.
	bool enforceActKeys = false;
	act_lock::Mode enforceActKeysMode = act_lock::Mode::WarpToTown;
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
		// What happens to potions picked up while potions are locked.
		potion_lock::Mode potionMode = potion_lock::Mode::SetAside;
		// The charges of the Tome of Town Portal that Town Portal Access brings (0 = none).
		int portalTomeCharges = 20;
	} locks;
	// Trap tuning; the defaults are the traps from before these were options.
	struct Traps {
		bool waitForTown = true;
		int  ambushSize = 6;
		// Ambushes bring the area's own monsters instead of the act's.
		bool areaAmbush = false;
		int  taxPerLevel = 1000;
		int  junkSize = 40;
	} traps;
	// Deaths are shared with the other DeathLink players.
	bool deathLink   = false;
	randomizer::Settings randomizer;
	// D2RLoader settings d2r-ap-setup writes as the mod's overrides: dotted
	// path ("d2rcore.stash.add_shared_tabs") and TOML value ("50", "true").
	std::vector<std::pair<std::string, std::string>> loaderSettings;
};

// The received-item list with the state it belongs to, taken under one lock so a delivery pass
// can't mix one session's items with another's settings or flags.
struct ItemSnapshot {
	std::vector<NetworkItem> items;
	SlotData slots;
	uint64_t generation = 0;
	uint64_t session    = 0;
	// Whether the list is the server's full list for this session (see Client::ItemsSynced).
	bool synced = false;
};

// The games (of those in the multiworld) whose data package to ask for: ones we hold no names for,
// and ones whose checksum in RoomInfo differs from the one we hold.
auto DataPackageGames(const std::set<std::string>& games, const std::map<std::string, std::string>& roomChecksums,
	const std::map<std::string, std::string>& held) -> std::vector<std::string>;

// Who a PrintJSON line is for: us (an item we found or got, or any other message), the
// multiworld (an item between two other players, shown quieter), or no one.
enum class PrintAudience { Own, Others, Hidden };

// Sorts a PrintJSON line by its type, the item's receiving and finding slots and its flags
// (bit 0: progression), for our slot and the other_sends setting. Hints between other
// players stay hidden; the server doesn't send those to us anyway.
auto PrintJsonAudience(const std::string& type, int receiver, int finder, int flags, int me, OtherSends setting) -> PrintAudience;

// Other players' send lines taken from the client, and how many it let go (logged) since.
struct OtherSendLines {
	std::vector<std::string> lines;
	size_t dropped = 0;
};

// Other players' send lines waiting for the chat, behind our own. A burst (a release or
// collect) shows its first few, then one line counting the rest, which are only in the log.
class OtherSendsFeed {
public:
	static constexpr size_t Shown = 6;

	void Add(std::string line);
	void Skip(size_t count);
	auto Empty() const -> bool;
	// Up to `room` lines for the chat; the count line comes once the shown ones are out.
	auto Take(size_t room) -> std::vector<std::string>;

private:
	std::deque<std::string> lines_;
	size_t skipped_ = 0;
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
	// Registers every hint found in game again (see HintPackets); a hint whose
	// first send hit a closed socket reaches the server this way.
	void ResendHints(const std::vector<HintEntry>& own, const std::vector<int64_t>& others);
	// Tells the other DeathLink players that we died.
	void SendDeath(const std::string& cause);

	auto State() const -> ConnectionState;
	auto StatusText() const -> std::string;
	// Bumped immediately on every Connect/Disconnect request, before any handshake.
	auto TargetGeneration() const -> uint64_t;
	// The accepted connection belongs to the latest requested target.
	auto HasCurrentSession() const -> bool;
	// Bumped on every successful Connected packet.
	auto SessionGeneration() const -> uint64_t;
	// The session generation while Connected, else 0: the session there is to bind to. Between an
	// ap-connect and the new Connected there's none, so the old seed's save isn't loaded again.
	auto ConnectedSession() const -> uint64_t;
	auto Seed() const -> std::string;
	auto Slot() const -> std::string;
	auto Slots() const -> SlotData;
	auto Checked() const -> std::set<int64_t>;
	auto CheckedCount() const -> size_t;
	auto LocationCount() const -> size_t;
	// Our locations the server hasn't seen checked, in id order.
	auto Missing() const -> std::vector<int64_t>;
	// Whether the item list is the server's full list for this session (a ReceivedItems with index 0
	// has arrived since Connected). Until then it's empty and nothing should be delivered.
	auto ItemsSynced() const -> bool;
	// The list, its generation, the session and the synced flag, all from one moment.
	auto Snapshot() const -> ItemSnapshot;
	// Bumped whenever the received-item list changes.
	auto ItemsGeneration() const -> uint64_t;
	auto Items() const -> std::vector<NetworkItem>;
	auto TakeMessages() -> std::vector<std::string>;
	// Which sends between other players reach TakeOtherSends (other_sends).
	void SetOtherSends(OtherSends setting);
	// Sends between other players since the last call, kept apart so a release can't push our
	// own messages out.
	auto TakeOtherSends() -> OtherSendLines;
	// Deaths from other DeathLink players since the last call, as messages.
	auto TakeDeaths() -> std::vector<std::string>;
	auto ItemName(int64_t item, int player) const -> std::string;
	auto PlayerName(int slot) const -> std::string;
	auto LocationName(int64_t location, int player) const -> std::string;
	auto MySlot() const -> int;
	// What our own unchecked locations hold, scouted when we connect.
	auto Scouted() const -> std::map<int64_t, NetworkItem>;

	// Processes one server message. The network thread calls it; tests feed it
	// packets directly. The network thread passes the connection target the
	// socket was opened for; a Connect or Disconnect since makes its RoomInfo and
	// Connected stale, and they're dropped.
	void Handle(const std::string& text, std::optional<uint64_t> target = std::nullopt);

private:
	void Run();
	// Runs the connected socket until it ends, and says why it ended.
	auto Session(uint64_t target) -> std::string;
	// Sets Idle with the "Disconnected" status; mutex_ must be held.
	void SetDisconnected();
	void HandlePacket(const void* packet, std::optional<uint64_t> target);
	// Whether a packet read for this connection target belongs to one since let go (mutex_ held).
	// Checked under the same lock as each change a packet makes, so a switch can't slip between.
	auto IsStale(std::optional<uint64_t> target) const -> bool;
	void Send(const std::string& text);
	void Notify(const std::string& message, std::optional<uint64_t> target = std::nullopt);
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
	uint64_t acceptedTargetGeneration_ = 0;
	std::string seed_;
	int team_ = 0, mySlot_ = 0;
	SlotData slotData_;
	std::set<int64_t> checked_;
	// Every location in our world, checked or not.
	std::set<int64_t> locations_;
	std::vector<NetworkItem> items_;
	bool itemsSynced_ = false;
	uint64_t itemsGeneration_ = 0;
	std::deque<std::string> messages_;
	OtherSends otherSends_ = OtherSends::Progression;
	std::deque<std::string> otherSendLines_;
	size_t otherSendsDropped_ = 0;
	std::vector<std::string> deaths_;
	std::map<int64_t, NetworkItem> scouted_;
	std::unordered_map<int, std::string> playerNames_;
	std::unordered_map<int, std::string> playerGames_;
	std::unordered_map<std::string, std::unordered_map<int64_t, std::string>> itemNames_;
	std::unordered_map<std::string, std::unordered_map<int64_t, std::string>> locationNames_;
	// Data package checksums of the names we hold. The names are kept across reconnects, so an
	// unchanged game isn't downloaded again.
	std::map<std::string, std::string> dataChecksums_;
};

// One message registering hints found in game: a CreateHints per own hint (one
// command each, since the server drops the whole command on one bad location)
// and a LocationScouts with create_as_hint 2 for our locations holding other
// players' items. The server keeps hints it already has, so sending them again
// changes nothing. Empty when there's nothing to send.
auto HintPackets(const std::vector<HintEntry>& own, const std::vector<int64_t>& others) -> std::string;

} // namespace ap
