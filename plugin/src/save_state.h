#pragma once

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace ap {

// A D2RCore console command a reward still has to run (gold, experience, skadd...).
// item is the index of the received item it came from, or -1 for none.
struct PendingCommand {
	int64_t     item = -1;
	std::string text;

	bool operator==(const PendingCommand&) const = default;
};

// What the plugin remembers about one seed and slot between sessions.
struct SaveState {
	// Every check this client has found, sent or not.
	std::set<int64_t> checks;
	// How many received items have had their filler handed out.
	size_t fillersDelivered = 0;
	bool goalReached = false;
	// Highest character level seen, for scaling loot.
	int level = 0;
	// The class of the seed's character (0 Amazon ... 7 Warlock), or -1 until one claims the seed.
	// Rewards only go to a character of this class.
	int playerClass = -1;
	// Monster kills per difficulty * 10 + act, for the kill-count checks.
	std::map<int, int> kills;
	// Secret Cow Level cow kills per difficulty.
	std::map<int, int> cowKills;
	// Item seeds of the uniques and set items you found (not rewards), for the find-count checks.
	std::set<uint32_t> foundUniques;
	std::set<uint32_t> foundSets;
	// Reward copies handed out per item code. Runes, gems and other simple items
	// keep no seed in the character's save, so after a reload a reward one has lost
	// its marker and looks like a find (#108); FoundCodes leaves these out.
	std::map<uint32_t, int> rewardCopies;
	// Hints found in game: indexes into the seed's hint list, and our own locations
	// hinted for other players. Scrolls of Prophecy bought (the next tier to buy).
	std::set<int> ownHints;
	std::set<int64_t> otherHints;
	int prophecies = 0;
	// Whether the character got its Archipelago Ledger (ap-ledger makes another).
	bool ledgerGiven = false;
	// Tree mode: skills unlocked in the tree (skadd sets a level, so each only
	// once), and skill points handed out for extra copies and Skill Points.
	std::set<uint32_t> unlockedSkills;
	int skillPointsGiven = 0;
	// How many received items have had an on-screen banner.
	size_t notifiedItems = 0;
	// Whether Town Portal Access has brought its Tome of Town Portal.
	bool portalTomeGiven = false;
	// Item codes of the potions put aside from the inventory while potions are
	// locked (one entry each), handed back with Potion Access.
	std::vector<uint32_t> heldPotions;
	// Reward commands not run yet, in order. Each one stays until D2RCore accepts it,
	// so a crash or a refused command doesn't lose it.
	std::vector<PendingCommand> commands;
	// fillersDelivered when a game was last left, which saves the character.
	size_t checkpoint = 0;
	// Reward commands that ran since then: a crash may undo them too.
	std::vector<PendingCommand> ran;
	// Items [lostFrom, lostTo) were handed out in a game D2R never left (a crash), so the
	// character may have lost them, and lostCommands ran in it. ap-redeliver sets redeliver
	// to hand them out again; lostFrom moves up as it does, and redeliverMark is where it
	// stood when the character was last saved, so another crash starts it over from there.
	size_t lostFrom = 0;
	size_t lostTo = 0;
	std::vector<PendingCommand> lostCommands;
	bool redeliver = false;
	size_t redeliverMark = 0;

	bool Load(const std::filesystem::path& path);
	bool Save(const std::filesystem::path& path) const;

	// On the first bind after starting: true if the last game with rewards was never left,
	// which records them as lost (lostFrom/lostTo, lostCommands) for ap-redeliver, or
	// rewinds a redelivery under way.
	bool DetectUncleanExit();
	// Leaving a game: the character is saved with everything handed out so far.
	void Checkpoint();
	// ap-redeliver: hand the lost rewards out again, queue the lost commands again, and drop
	// the commands that the items handed out again make anew. False if nothing was lost.
	bool StartRedelivery();
	// Moves the commands that ran (the first matching entry for each) from commands to ran.
	void FinishCommands(const std::vector<PendingCommand>& done);

private:
	bool InLostRange(const PendingCommand& command) const;
	void RequeueLostCommands();
};

// The item codes the character found itself, from how many copies of each it holds
// without the reward marker. Some of those may be rewards that lost the marker in a
// reload: at most the copies held when the game was joined (rewards handed out since
// still carry it), and at most the rewardCopies ever handed out. A code counts once it
// holds more than that, so using up a reward doesn't hide a later find.
inline auto FoundCodes(const std::map<uint32_t, int>& held, const std::map<uint32_t, int>& heldAtJoin, const std::map<uint32_t, int>& rewardCopies)
	-> std::set<uint32_t> {
	const auto count = [](const std::map<uint32_t, int>& counts, uint32_t code) {
		const auto found = counts.find(code);
		return found != counts.end() ? found->second : 0;
	};
	std::set<uint32_t> codes;
	for (const auto& [code, copies] : held) {
		if (copies > std::min(count(heldAtJoin, code), count(rewardCopies, code))) {
			codes.insert(code);
		}
	}
	return codes;
}

} // namespace ap
