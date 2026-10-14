#pragma once

// Which character a seed's rewards may go to. The game client says the character's
// class and name as a game begins (#155); without that (a game build where it isn't
// found), the class from a kill or the Codex is all there is to tell characters apart.

#include <string_view>

namespace ap::character {

enum class Verdict {
	// This game's class isn't known yet (no kill by the player, no Codex charm).
	Unknown,
	// The seed's character, or the first one to claim a seed with no class yet.
	Match,
	// Another character: a mule or another seed's character.
	Mismatch,
};

// seedClass is the class the seed is bound to (-1: none yet, so this character claims it);
// gameClass is this game's character's class (-1 while unknown). Class ids 0 Amazon ... 7 Warlock.
// seedName and gameName are the seed's character's name and this game's, empty when not known:
// when both are known, only that character matches, not every one of its class.
inline auto Check(int seedClass, int gameClass, std::string_view seedName = {}, std::string_view gameName = {}) -> Verdict {
	if (gameClass < 0) {
		return Verdict::Unknown;
	}
	if (seedClass >= 0 && seedClass != gameClass) {
		return Verdict::Mismatch;
	}
	return seedName.empty() || gameName.empty() || seedName == gameName ? Verdict::Match : Verdict::Mismatch;
}

} // namespace ap::character
