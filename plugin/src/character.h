#pragma once

// Which character a seed's rewards may go to. D2RLoader doesn't expose the
// character's name, so the class is what tells characters apart.

namespace ap::character {

enum class Verdict {
	// This game's class isn't known yet (no kill by the player, no Codex charm).
	Unknown,
	// The seed's character, or the first one to claim a seed with no class yet.
	Match,
	// Another class: a mule or another seed's character.
	Mismatch,
};

// seedClass is the class the seed is bound to (-1: none yet, so this character claims it);
// gameClass is this game's character's class (-1 while unknown). Class ids 0 Amazon ... 7 Warlock.
inline auto Check(int seedClass, int gameClass) -> Verdict {
	if (gameClass < 0) {
		return Verdict::Unknown;
	}
	return seedClass < 0 || seedClass == gameClass ? Verdict::Match : Verdict::Mismatch;
}

} // namespace ap::character
