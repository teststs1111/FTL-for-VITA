#pragma once
#include <cstdint>
#include <vector>
#include <string>

namespace wormhole {
class Audio {
public:
    Audio() = default;
    ~Audio();
    bool init();
    void shutdown();
    void update();
    bool playPcm16Stereo(const std::vector<std::int16_t>& samples, float volume = 1.0f);
    bool playWav(const std::vector<std::uint8_t>& bytes, float volume = 1.0f);
    bool playOgg(const std::vector<std::uint8_t>& bytes, float volume = 1.0f, bool loop = false);
    bool playAsset(const std::vector<std::uint8_t>& bytes, const std::string& name, float volume = 1.0f, bool loop = false);
    void stopMusic();
    bool initialized() const { return initialized_; }
private:
    struct Voice { std::vector<std::int16_t> samples; std::size_t frame{0}; float volume{1.0f}; bool loop{false}; bool music{false}; bool mono{false}; };
    bool initialized_{false};
    std::vector<Voice> voices_;
#ifdef __vita__
    int port_{-1};
    static constexpr int sampleRate_ = 48000;
    static constexpr int bufferFrames_ = 1024;
    std::vector<std::int16_t> outputBuffer_;
#endif
};
} // namespace wormhole
