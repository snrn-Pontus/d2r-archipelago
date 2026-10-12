#include "sprite.h"

#include <cstring>

namespace ap::sprite {

namespace {

constexpr size_t HeaderSize = 40;

auto ReadU32(const std::string& bytes, size_t offset) -> uint32_t {
	uint32_t value = 0;
	std::memcpy(&value, bytes.data() + offset, sizeof(value));
	return value;
}

} // namespace

auto Parse(std::string bytes) -> std::optional<Sheet> {
	if (bytes.size() < HeaderSize || bytes.compare(0, 4, "SpA1") != 0) {
		return std::nullopt;
	}
	Sheet sheet;
	sheet.width = ReadU32(bytes, 8);
	sheet.height = ReadU32(bytes, 12);
	sheet.frames = ReadU32(bytes, 20);
	if (sheet.frames == 0 || sheet.width % sheet.frames != 0
	    || bytes.size() < HeaderSize + static_cast<size_t>(sheet.width) * sheet.height * 4) {
		return std::nullopt;
	}
	sheet.bytes = std::move(bytes);
	return sheet;
}

bool CopyFrame(const Sheet& source, uint32_t from, Sheet& target, uint32_t to) {
	const uint32_t frameWidth = source.FrameWidth();
	if (from >= source.frames || to >= target.frames || frameWidth != target.FrameWidth() || source.height != target.height) {
		return false;
	}
	const size_t span = static_cast<size_t>(frameWidth) * 4;
	for (uint32_t y = 0; y < source.height; ++y) {
		const size_t sourceRow = HeaderSize + (static_cast<size_t>(y) * source.width + static_cast<size_t>(from) * frameWidth) * 4;
		const size_t targetRow = HeaderSize + (static_cast<size_t>(y) * target.width + static_cast<size_t>(to) * frameWidth) * 4;
		std::memcpy(target.bytes.data() + targetRow, source.bytes.data() + sourceRow, span);
	}
	return true;
}

} // namespace ap::sprite
