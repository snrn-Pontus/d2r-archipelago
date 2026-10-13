#pragma once

#include <string>

// Banner sounds: game sounds that d2r-ap-setup converts from the game files
// (FLAC) to WAV, for the plugin to play with the banners.

namespace ap::sounds {

struct Sound {
	const wchar_t* file;   // in Folder
	const char*    source; // in the game files
};

// Under the game folder.
inline constexpr const wchar_t* Folder = L"d2rloader\\archipelago\\sounds";

inline constexpr Sound Important { L"important.wav", "data\\global\\sfx\\cursor\\questdone.flac" };
inline constexpr Sound Trap { L"trap.wav", "data\\global\\sfx\\cursor\\hostile.flac" };
inline constexpr Sound Item { L"item.wav", "data\\global\\sfx\\item\\gem.flac" };

inline constexpr Sound All[] = { Important, Trap, Item };

// Played quieter than the game files, since they ignore the game's volume.
inline constexpr float Volume = 0.5F;

// Scales the samples of a 16-bit PCM WAV file in place by percent / 100,
// clipping at full scale. Returns false, leaving it alone, for anything else.
auto ScaleWav(std::string& wav, int percent) -> bool;

} // namespace ap::sounds
