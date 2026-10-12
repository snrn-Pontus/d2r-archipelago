#pragma once

// Personalized rewards: a unique or set item from another player carries their
// name, like Anya's reward ("Bob's Harlequin Crest"). The SDK can't set it, so
// the plugin writes D2's item data through editNativeItem. These are the pure
// parts: the name rules and the layout check that guards the write.

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace ap::personal {

// D2R's item unit and item data, from D2 1.10's layout moved for 64-bit
// pointers. Only written after CheckLayout agrees with the item's ItemInfo.
inline constexpr size_t   UnitType         = 0x00;
inline constexpr size_t   UnitClassId      = 0x04;
inline constexpr size_t   UnitId           = 0x08;
inline constexpr size_t   UnitData         = 0x10;
inline constexpr size_t   UnitSize         = 0x18;
inline constexpr uint32_t ItemUnitType     = 4;
inline constexpr size_t   DataQuality      = 0x00;
inline constexpr size_t   DataFlags        = 0x18;
inline constexpr size_t   DataName         = 0x5A;
inline constexpr size_t   NameSize         = 16;
inline constexpr size_t   DataSize         = 0x100;
inline constexpr uint32_t FlagIdentified   = 0x00000010U;
inline constexpr uint32_t FlagEthereal     = 0x00400000U;
inline constexpr uint32_t FlagPersonalized = 0x01000000U;

// A player name as D2 can store it: letters, digits, spaces, '-' and '_', at
// most 15 characters. Empty when nothing usable is left.
auto Name(std::string_view player) -> std::string;

// Why the item data doesn't look like the layout above, or empty when it does:
// the quality, identified and ethereal flags must match, and the name slot must
// be empty (or hold a name already, on a personalized item).
auto CheckLayout(std::span<const uint8_t> data, uint32_t quality, bool identified, bool ethereal) -> std::string;

// The bytes as hex, 16 per line, each line starting with its offset.
auto HexDump(std::span<const uint8_t> bytes) -> std::string;

}
