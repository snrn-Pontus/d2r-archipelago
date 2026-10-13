#pragma once

#include <chrono>
#include <string_view>

namespace ap::repairs {

inline auto PanelName(std::string_view name) -> std::string_view {
	if (name.size() >= 2 && name.front() == '"' && name.back() == '"') {
		name = name.substr(1, name.size() - 2);
	}
	return name;
}

inline auto IsVendorPanel(std::string_view name) -> bool {
	return PanelName(name).starts_with("Vendor");
}

// A repair offer belongs to one freshly intercepted vendor interaction. It is
// consumed before dispatch, and cancelled on closing, leaving town, death,
// game/slot changes, or unlocking merchants. Caller holds the plugin mutex.
class Offer {
public:
	using Clock = std::chrono::steady_clock;
	void Begin(Clock::time_point now) { expires_ = now + std::chrono::seconds(30); }
	void Cancel() { expires_ = {}; }
	auto Ready(bool eligible, Clock::time_point now) const -> bool { return eligible && now < expires_; }
	auto Take(bool eligible, Clock::time_point now) -> bool {
		const bool ready = Ready(eligible, now);
		Cancel();
		return ready;
	}

private:
	Clock::time_point expires_ {};
};

} // namespace ap::repairs
