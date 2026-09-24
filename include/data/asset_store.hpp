#pragma once
#include "data/ftl_dat.hpp"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace wormhole {

class AssetStore {
public:
    bool openArchive(const std::string& path);
    bool isOpen() const { return archive_.isOpen(); }

    const std::vector<std::uint8_t>* getBytes(const std::string& name);\n    std::vector<std::string> fileNames() const;
    void clearCache();

private:
    FtlDat archive_;
    std::unordered_map<std::string, std::vector<std::uint8_t>> byteCache_;
};

}
