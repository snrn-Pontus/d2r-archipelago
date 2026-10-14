#pragma once

#include <functional>
#include <string_view>
#include <utility>

// What Enforce Act Keys does when you walk into an act you don't have the key
// for (the seed's enforce_act_keys_mode): warp you back to a town, or only say so.

namespace ap::act_lock {

enum class Mode { WarpToTown, MessageOnly };

// The slot data's enforce_act_keys_mode key; anything else warps.
inline auto ModeFromKey(std::string_view key) -> Mode {
	return key == "message_only" ? Mode::MessageOnly : Mode::WarpToTown;
}

// The act (0 to 4 within a difficulty) whose town a locked act sends you to:
// the furthest open act below it, or Act I, which is always open.
inline auto TownBelow(int act, const std::function<bool(int)>& open) -> int {
	for (int below = act - 1; below > 0; --below) {
		if (open(below)) {
			return below;
		}
	}
	return 0;
}

// Message only says so once each time you walk into a locked act, not in every
// area of it. Acts are numbered across difficulties (difficulty * 5 + act).
class Notice {
public:
	// An area in this act was entered; true if that's a new walk into a locked act.
	auto Enter(int act, bool locked) -> bool {
		if (!locked) {
			last_ = -1;
			return false;
		}
		return std::exchange(last_, act) != act;
	}

	void Reset() { last_ = -1; }

private:
	int last_ = -1;
};

} // namespace ap::act_lock
