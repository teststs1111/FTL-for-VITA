#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace wormhole::bxml {

struct Node {
    std::string name;
    std::unordered_map<std::string, std::string> attributes;
    std::string text;
    std::vector<Node> children;
};

// Lightweight XML reader for Tachyon/FTL XML assets.
// The name is retained for compatibility with the existing data layer.
Node read(const std::vector<std::uint8_t>& data);

}
