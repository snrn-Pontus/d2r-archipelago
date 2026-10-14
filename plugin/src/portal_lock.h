#pragma once

#include "loot.h"

#include <algorithm>
#include <cstdint>
#include <optional>

// The Town Portal lock: scrolls and tomes on the character are set aside while
// it's on, and Town Portal Access hands them back with a Tome of Town Portal
// holding the seed's tp_access_tome_charges (none at 0).

namespace ap::portal_lock {

// A tome holds at most this many charges.
inline constexpr int MaxCharges = 20;

inline auto IsTome(uint32_t code) -> bool {
	return code == loot::CodeFromText("tbk");
}

inline auto IsPortal(uint32_t code) -> bool {
	return code == loot::CodeFromText("tsc") || IsTome(code);
}

// A scroll or tome set aside, with its charges (one for a scroll).
struct Held {
	uint32_t code    = 0;
	int      charges = 1;

	bool operator==(const Held&) const = default;
};

// What's set aside for a scroll or tome with this quantity. An empty tome goes
// too: the cow level recipe takes one whatever its charges.
inline auto SetAside(uint32_t code, int quantity) -> std::optional<Held> {
	if (!IsPortal(code)) {
		return std::nullopt;
	}
	if (!IsTome(code)) {
		return Held { code, 1 };
	}
	return Held { code, std::clamp(quantity, 0, MaxCharges) };
}

// The tome Town Portal Access brings: once, and none at 0 charges.
inline auto AccessTome(bool given, int charges) -> std::optional<Held> {
	if (given || charges <= 0) {
		return std::nullopt;
	}
	return Held { loot::CodeFromText("tbk"), std::min(charges, MaxCharges) };
}

} // namespace ap::portal_lock
