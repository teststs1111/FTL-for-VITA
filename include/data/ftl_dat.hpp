#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace wormhole {

class FtlDat {
public:
    bool open(const std::string& path);
    bool isOpen() const { return open_; }

    std::vector<std::uint8_t> readFile(const std::string& name) const;
    bool contains(const std::string& name) const;
    std::vector<std::string> fileNames() const;

private:
    struct Entry { std::uint32_t offset{}; std::uint32_t length{}; };
    bool open_{false};
    std::string path_;
    std::unordered_map<std::string, Entry> files_;
};

}
