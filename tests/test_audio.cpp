#include "audio/audio.hpp"

#include <cassert>
#include <cstdint>
#include <vector>

namespace {
void append16(std::vector<std::uint8_t>& out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xff));
    out.push_back(static_cast<std::uint8_t>(value >> 8));
}
void append32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8)
        out.push_back(static_cast<std::uint8_t>((value >> shift) & 0xff));
}
void appendTag(std::vector<std::uint8_t>& out, const char* tag) {
    out.insert(out.end(), tag, tag + 4);
}
std::vector<std::uint8_t> chunk(const char* tag, const std::vector<std::uint8_t>& payload) {
    std::vector<std::uint8_t> out;
    appendTag(out, tag);
    append32(out, static_cast<std::uint32_t>(payload.size()));
    out.insert(out.end(), payload.begin(), payload.end());
    if (payload.size() & 1u) out.push_back(0);
    return out;
}
std::vector<std::uint8_t> pcmFormat(std::uint16_t channels, std::uint32_t rate,
                                    std::uint16_t bits) {
    std::vector<std::uint8_t> out;
    append16(out, 1);
    append16(out, channels);
    append32(out, rate);
    append32(out, rate * channels * (bits / 8));
    append16(out, static_cast<std::uint16_t>(channels * (bits / 8)));
    append16(out, bits);
    return out;
}
std::vector<std::uint8_t> wav(const std::vector<std::vector<std::uint8_t>>& chunks) {
    std::vector<std::uint8_t> body;
    for (const auto& part : chunks) body.insert(body.end(), part.begin(), part.end());
    std::vector<std::uint8_t> out;
    appendTag(out, "RIFF");
    append32(out, static_cast<std::uint32_t>(body.size() + 4));
    appendTag(out, "WAVE");
    out.insert(out.end(), body.begin(), body.end());
    return out;
}
}

int main() {
    wormhole::Audio audio;
    assert(audio.init());

    const auto fmt8 = chunk("fmt ", pcmFormat(1, 24000, 8));
    const auto validData = chunk("data", {128, 160, 96, 128});

    // Standard fmt-before-data ordering remains supported.
    assert(audio.playWav(wav({fmt8, validData})));

    // A valid RIFF/WAVE may place data before fmt; the parser must scan all chunks.
    assert(audio.playWav(wav({validData, fmt8})));

    // Missing format metadata must be rejected safely.
    assert(!audio.playWav(wav({validData})));

    // PCM data must contain complete sample frames.
    const auto fmt16 = chunk("fmt ", pcmFormat(1, 24000, 16));
    assert(!audio.playWav(wav({fmt16, chunk("data", {0x34})})));

    // A truncated chunk payload must fail validation.
    auto truncated = wav({fmt8, validData});
    truncated.pop_back();
    assert(!audio.playWav(truncated));

    audio.update();
    audio.shutdown();
    return 0;
}
