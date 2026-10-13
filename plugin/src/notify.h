#pragma once

#include <string>

namespace ap::notify {

// On-screen banners for Archipelago events. D2RLoader's overlay draws nothing
// (1.3.1-beta: every call succeeds, nothing shows), so the banners are a small
// click-through window of our own, owned by the game window and placed over it,
// near the top centre. They show in windowed and borderless modes.

enum class Style {
	Info,      // a received item
	Important, // act keys, locks, skills
	Trap,
	Check,     // a check you sent
};

// Where d2r-ap-setup put the banner sounds (sounds.h), and whether to play
// them: gold banners play the quest-done chime, traps the hostile warning,
// other items the gem sound; checks are silent. volume is in percent of the
// files' own level (banner_volume). Call before the first Show.
void SetSounds(const std::wstring& folder, bool enabled, int volume);

// Stops a banner sound mid-play, which reads our memory: call before unloading.
void StopSounds();

// Queues a banner line. Thread-safe.
void Show(const std::string& text, Style style);

// Where the status line sits on the game window.
enum class Corner { TopLeft, Top, TopRight, BottomLeft, BottomRight };

// The always-on status line; empty hides it. Thread-safe.
void SetStatus(const std::string& text, Corner corner);

// Whether Tick has work: banner lines or the status line. Thread-safe.
auto Busy() -> bool;

// UI thread, every frame or so while Busy: finds the game window, creates the
// banner window, and lays out, fades and hides the lines.
void Tick();

// UI thread: destroys the window.
void Shutdown();

} // namespace ap::notify
