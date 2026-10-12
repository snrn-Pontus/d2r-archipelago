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

// Queues a banner line. Thread-safe.
void Show(const std::string& text, Style style);

// Whether Tick has work: lines waiting or showing. Thread-safe.
auto Busy() -> bool;

// UI thread, every frame or so while Busy: finds the game window, creates the
// banner window, and lays out, fades and hides the lines.
void Tick();

// UI thread: destroys the window.
void Shutdown();

} // namespace ap::notify
