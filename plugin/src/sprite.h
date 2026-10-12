#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace ap::sprite {

// A D2R .sprite: "SpA1", the strip's width (offset 8), height (12) and frame
// count (20), then RGBA pixels from offset 40, the frames side by side.
struct Sheet {
	std::string bytes;
	uint32_t    width  = 0;
	uint32_t    height = 0;
	uint32_t    frames = 0;

	auto FrameWidth() const -> uint32_t { return frames > 0 ? width / frames : 0; }
};

// Nothing when the bytes aren't a sprite this code understands.
auto Parse(std::string bytes) -> std::optional<Sheet>;

// Copies frame `from` of `source` over frame `to` of `target`. Both sheets
// need the same frame size. False when a frame is out of range.
bool CopyFrame(const Sheet& source, uint32_t from, Sheet& target, uint32_t to);

// Clears every shape thinner than `kernel` pixels (a morphological opening of
// the visible pixels), and what's left of a shape smaller than `minSize` in
// width or height, below the first fully transparent row (the header stays).
// Skill tree art: the arrows go, the skill and level boxes stay.
void RemoveThinShapes(Sheet& sheet, uint32_t kernel, uint32_t minSize);

} // namespace ap::sprite
