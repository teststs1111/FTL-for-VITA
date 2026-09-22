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

// Tachyon-compatible BXML reader.
// Type IDs: 0=text, 1=child element, 2=end-of-element.
Node read(const std::vector<std::uint8_t>& data);

}
