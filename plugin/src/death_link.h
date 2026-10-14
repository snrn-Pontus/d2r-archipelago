#pragma once

#include "loot.h"

#include <algorithm>
#include <cstdint>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// What a DeathLink from another player does to this one (the seed's
// death_link_mode), and how many of this player's deaths it takes to send one
// (death_link_amnesty).

namespace ap::death_link {

enum class Mode { Die, OneHp, LoseGold, RandomTrap };

// The slot data's death_link_mode key; anything else dies, as before the option.
inline auto ModeFromKey(std::string_view key) -> Mode {
	return key == "one_hp"        ? Mode::OneHp
	       : key == "lose_gold"   ? Mode::LoseGold
	       : key == "random_trap" ? Mode::RandomTrap
	                              : Mode::Die;
}

// A trap a DeathLink can set off, with its weight in the seed.
struct TrapWeight {
	loot::Loot trap = loot::Loot::AmbushTrap;
	int        weight = 0;

	bool operator==(const TrapWeight&) const = default;
};

// The traps that go off through console commands. A Junk Backpack isn't one:
// it's handed out with the received items.
inline auto IsCommandTrap(loot::Loot loot) -> bool {
	return loot >= loot::Loot::AmbushTrap && loot <= loot::Loot::MercenaryDownTrap;
}

// The trap a random_trap DeathLink sets off, by the seed's trap weights. With
// none usable, any of the default ones (not Mercenary Down) is as likely.
inline auto PickTrap(std::span<const TrapWeight> weights, std::mt19937& random) -> loot::Loot {
	std::vector<loot::Loot> traps;
	std::vector<int>        odds;
	for (const auto& [trap, weight] : weights) {
		if (IsCommandTrap(trap) && weight > 0) {
			traps.push_back(trap);
			odds.push_back(weight);
		}
	}
	if (traps.empty()) {
		traps = { loot::Loot::AmbushTrap, loot::Loot::PoisonCloudTrap, loot::Loot::FirestormTrap, loot::Loot::ManaDrainTrap,
			      loot::Loot::TaxCollectorTrap };
		odds.assign(traps.size(), 1);
	}
	std::discrete_distribution<size_t> pick(odds.begin(), odds.end());
	return traps[pick(random)];
}

// The D2RCore console commands for a DeathLink in the die, one_hp and
// lose_gold modes, and the line that says what it did (empty for die, whose
// death speaks for itself). A random_trap one queues a trap instead, so it has none.
struct Penalty {
	std::vector<std::string> commands;
	std::string              message;
};
inline auto PenaltyFor(Mode mode, int level, int goldPerLevel) -> Penalty {
	switch (mode) {
		case Mode::Die:   return { { "killme" }, "" };
		// depletehealth sets life to this many points, clamped to the character's maximum.
		case Mode::OneHp: return { { "depletehealth 1" }, "DeathLink leaves you with 1 life." };
		case Mode::LoseGold: {
			const int amount = loot::TaxAmount(level, goldPerLevel);
			return { { "gold -" + std::to_string(amount) }, "DeathLink takes up to " + std::to_string(amount) + " gold." };
		}
		case Mode::RandomTrap: return {};
	}
	return {};
}

// Counts one of this player's deaths towards the next DeathLink sent: true when
// this death is sent (every Nth, every one with 1), which starts the count over.
// deaths is how many went unsent since the last one sent.
inline auto CountDeath(int& deaths, int every) -> bool {
	deaths = std::max(deaths, 0) + 1;
	if (deaths < std::max(every, 1)) {
		return false;
	}
	deaths = 0;
	return true;
}

} // namespace ap::death_link
