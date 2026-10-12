#include "sounds.h"

#include <algorithm>
#include <cstdint>
#include <cstring>

namespace ap::sounds {

namespace {

auto U16(const std::string& data, size_t at) -> uint16_t {
	uint16_t value = 0;
	std::memcpy(&value, data.data() + at, 2);
	return value;
}

auto U32(const std::string& data, size_t at) -> uint32_t {
	uint32_t value = 0;
	std::memcpy(&value, data.data() + at, 4);
	return value;
}

} // namespace

auto ScaleWav(std::string& wav, int percent) -> bool {
	if (wav.size() < 12 || wav.compare(0, 4, "RIFF") != 0 || wav.compare(8, 4, "WAVE") != 0) {
		return false;
	}
	bool   pcm16 = false;
	size_t data = 0, length = 0;
	for (size_t at = 12; at + 8 <= wav.size();) {
		const size_t size = U32(wav, at + 4);
		const size_t body = at + 8;
		if (size > wav.size() - body) {
			break;
		}
		if (wav.compare(at, 4, "fmt ") == 0 && size >= 16) {
			pcm16 = U16(wav, body) == 1 && U16(wav, body + 14) == 16;
		} else if (wav.compare(at, 4, "data") == 0) {
			data = body;
			length = size;
		}
		at = body + size + (size & 1); // chunks are word-aligned
	}
	if (!pcm16 || data == 0) {
		return false;
	}
	for (size_t i = data; i + 1 < data + length; i += 2) {
		int16_t sample = 0;
		std::memcpy(&sample, wav.data() + i, 2);
		sample = static_cast<int16_t>(std::clamp(sample * percent / 100, -32768, 32767));
		std::memcpy(wav.data() + i, &sample, 2);
	}
	return true;
}

} // namespace ap::sounds
