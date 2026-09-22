#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace wormhole {

struct RgbaImage {
    int width{0};
    int height{0};
    std::vector<std::uint8_t> pixels;
};

bool decodePng(const std::vector<std::uint8_t>& data, RgbaImage& out);
bool loadPng(const std::string& path, RgbaImage& out);

}
