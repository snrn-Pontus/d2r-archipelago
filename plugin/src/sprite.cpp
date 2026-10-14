#include "sprite.h"

#include <algorithm>
#include <cstring>
#include <deque>
#include <vector>

namespace ap::sprite {

namespace {

constexpr size_t HeaderSize = 40;

auto ReadU32(const std::string& bytes, size_t offset) -> uint32_t {
	uint32_t value = 0;
	std::memcpy(&value, bytes.data() + offset, sizeof(value));
	return value;
}

void WriteU32(std::string& bytes, size_t offset, uint32_t value) {
	std::memcpy(bytes.data() + offset, &value, sizeof(value));
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

void RemoveThinShapes(Sheet& sheet, uint32_t kernel, uint32_t minSize) {
	const size_t width = sheet.width, height = sheet.height;
	auto alpha = [&](size_t x, size_t y) { return static_cast<uint8_t>(sheet.bytes[HeaderSize + (y * width + x) * 4 + 3]); };
	size_t header = 5;
	for (; header < height; ++header) {
		size_t x = 0;
		while (x < width && alpha(x, header) == 0) {
			++x;
		}
		if (x == width) {
			break;
		}
	}
	// Summed-area table of a mask, and the count inside a window clipped to the sheet.
	auto table = [&](auto&& set) {
		std::vector<uint32_t> sums((width + 1) * (height + 1), 0);
		for (size_t y = 0; y < height; ++y) {
			uint32_t row = 0;
			for (size_t x = 0; x < width; ++x) {
				row += set(x, y) ? 1 : 0;
				sums[(y + 1) * (width + 1) + x + 1] = sums[y * (width + 1) + x + 1] + row;
			}
		}
		return sums;
	};
	const long radius = static_cast<long>(kernel / 2);
	auto count = [&](const std::vector<uint32_t>& sums, long x, long y) {
		const size_t x0 = static_cast<size_t>(std::max(x - radius, 0L)), y0 = static_cast<size_t>(std::max(y - radius, 0L));
		const size_t x1 = static_cast<size_t>(std::min(x + radius + 1, static_cast<long>(width)));
		const size_t y1 = static_cast<size_t>(std::min(y + radius + 1, static_cast<long>(height)));
		return sums[y1 * (width + 1) + x1] - sums[y0 * (width + 1) + x1] - sums[y1 * (width + 1) + x0] + sums[y0 * (width + 1) + x0];
	};
	const uint32_t full = static_cast<uint32_t>((2 * radius + 1) * (2 * radius + 1));
	const auto visible = table([&](size_t x, size_t y) { return alpha(x, y) > 0; });
	// Erode (the whole window visible), then dilate (any eroded pixel in the window).
	std::vector<uint8_t> eroded(width * height, 0);
	for (size_t y = 0; y < height; ++y) {
		for (size_t x = 0; x < width; ++x) {
			eroded[y * width + x] = count(visible, static_cast<long>(x), static_cast<long>(y)) == full ? 1 : 0;
		}
	}
	const auto erodedSums = table([&](size_t x, size_t y) { return eroded[y * width + x] != 0; });
	std::vector<uint8_t> opened(width * height, 0);
	for (size_t y = header; y < height; ++y) {
		for (size_t x = 0; x < width; ++x) {
			opened[y * width + x] = alpha(x, y) > 0 && count(erodedSums, static_cast<long>(x), static_cast<long>(y)) > 0 ? 1 : 0;
		}
	}
	// Keep the pieces at least minSize wide and tall.
	std::vector<uint8_t> keep(width * height, 0);
	std::vector<uint8_t> seen(width * height, 0);
	std::vector<size_t> piece;
	for (size_t start = header * width; start < width * height; ++start) {
		if (!opened[start] || seen[start]) {
			continue;
		}
		piece.clear();
		std::deque<size_t> queue { start };
		seen[start] = 1;
		size_t left = width, right = 0, top = height, bottom = 0;
		while (!queue.empty()) {
			const size_t at = queue.front();
			queue.pop_front();
			piece.push_back(at);
			const size_t x = at % width, y = at / width;
			left = std::min(left, x), right = std::max(right, x), top = std::min(top, y), bottom = std::max(bottom, y);
			const size_t next[] = { x > 0 ? at - 1 : at, x + 1 < width ? at + 1 : at, y > header ? at - width : at, y + 1 < height ? at + width : at };
			for (const size_t n : next) {
				if (opened[n] && !seen[n]) {
					seen[n] = 1;
					queue.push_back(n);
				}
			}
		}
		if (right - left + 1 >= minSize && bottom - top + 1 >= minSize) {
			for (const size_t at : piece) {
				keep[at] = 1;
			}
		}
	}
	for (size_t at = header * width; at < width * height; ++at) {
		if (!keep[at]) {
			std::memset(sheet.bytes.data() + HeaderSize + at * 4, 0, 4);
		}
	}
}



void AddLockedFrames(Sheet& sheet) {
	const uint32_t frameWidth = sheet.FrameWidth(), oldWidth = sheet.width, height = sheet.height;
	const uint32_t width = oldWidth * 2;
	std::string bytes = sheet.bytes.substr(0, HeaderSize);
	bytes.resize(HeaderSize + static_cast<size_t>(width) * height * 4);
	WriteU32(bytes, 8, width);
	WriteU32(bytes, 20, sheet.frames * 2);
	WriteU32(bytes, 32, width * height * 4);
	// A padlock in the middle of each frame: a body and the upper half of a ring, with a keyhole.
	const double size = std::min(frameWidth, height);
	const double middle = frameWidth / 2.0, bodyTop = height * 0.48, bodyBottom = height * 0.78, bodyHalf = size * 0.2;
	const double outer = size * 0.15, inner = size * 0.09;
	auto lock = [&](double x, double y) -> int {
		if (y >= bodyTop && y < bodyBottom && x >= middle - bodyHalf && x < middle + bodyHalf) {
			const double hole = (x - middle) * (x - middle) + (y - (bodyTop + bodyBottom) / 2) * (y - (bodyTop + bodyBottom) / 2);
			return hole < size * size * 0.0025 ? 2 : 1;
		}
		const double ring = (x - middle) * (x - middle) + (y - bodyTop) * (y - bodyTop);
		return y < bodyTop && ring < outer * outer && ring >= inner * inner ? 1 : 0;
	};
	for (uint32_t y = 0; y < height; ++y) {
		const char* from = sheet.bytes.data() + HeaderSize + static_cast<size_t>(y) * oldWidth * 4;
		char* to = bytes.data() + HeaderSize + static_cast<size_t>(y) * width * 4;
		std::memcpy(to, from, static_cast<size_t>(oldWidth) * 4);
		for (uint32_t x = 0; x < oldWidth; ++x) {
			const auto* pixel = reinterpret_cast<const uint8_t*>(from + static_cast<size_t>(x) * 4);
			auto* locked = reinterpret_cast<uint8_t*>(to + (static_cast<size_t>(oldWidth) + x) * 4);
			const auto grey = static_cast<uint8_t>((pixel[0] * 77 + pixel[1] * 150 + pixel[2] * 29) * 45 / (256 * 100));
			locked[0] = locked[1] = locked[2] = grey;
			locked[3] = pixel[3];
			switch (lock(x % frameWidth + 0.5, y + 0.5)) {
			case 1:
				locked[0] = 200, locked[1] = 185, locked[2] = 150, locked[3] = 255;
				break;
			case 2:
				locked[0] = locked[1] = locked[2] = 20, locked[3] = 255;
				break;
			default:
				break;
			}
		}
	}
	sheet.bytes = std::move(bytes);
	sheet.width = width;
	sheet.frames *= 2;
}

} // namespace ap::sprite
