#include "data/asset_store.hpp"
#include <algorithm>

namespace wormhole {

bool AssetStore::openArchive(const std::string& path) {
    clearCache();
    return archive_.open(path);
}

const std::vector<std::uint8_t>* AssetStore::getBytes(const std::string& name) {
    if (!archive_.isOpen()) return nullptr;

    const auto cached = byteCache_.find(name);
    if (cached != byteCache_.end()) return &cached->second;

    auto data = archive_.readFile(name);
    if (data.empty() && !archive_.contains(name)) return nullptr;

    auto [it, _] = byteCache_.emplace(name, std::move(data));
    return &it->second;
}

std::vector<std::string> AssetStore::fileNames() const {
    auto names = archive_.fileNames();
    std::sort(names.begin(), names.end());
    return names;
}

void AssetStore::clearCache() {
    byteCache_.clear();
}

}
