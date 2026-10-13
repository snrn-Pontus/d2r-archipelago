#pragma once

#include <chrono>

namespace ap::drops {

// Paces items dropped at the player's feet. D2R crashed (an hRoom assert in
// UnitRoom.cpp, #109) when a Tome of Town Portal and eight potions went to the
// ground in the same tick, so at most one item goes down per Gap, and only while
// the player is settled in an area. Whatever doesn't get a turn stays queued or
// held for a later pass. Game thread only.
class GroundGate {
public:
	using Clock = std::chrono::steady_clock;
	static constexpr auto Gap = std::chrono::seconds(1);
	// Whether one item may go to the ground now; a yes uses up the turn.
	auto Take(bool settled, Clock::time_point now) -> bool {
		if (!settled || now < next_) {
			return false;
		}
		next_ = now + Gap;
		return true;
	}

private:
	Clock::time_point next_ {};
};

} // namespace ap::drops
