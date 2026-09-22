#include "data/ftl_dat.hpp"
#include <fstream>

namespace wormhole {
namespace {
std::uint16_t be16(const std::uint8_t* p) { return (std::uint16_t(p[0]) << 8) | p[1]; }
std::uint32_t be32(const std::uint8_t* p) {
    return (std::uint32_t(p[0]) << 24) | (std::uint32_t(p[1]) << 16) |
           (std::uint32_t(p[2]) << 8) | p[3];
}
std::uint32_t be24(const std::uint8_t* p) {
    return (std::uint32_t(p[0]) << 16) | (std::uint32_t(p[1]) << 8) | p[2];
}
}

bool FtlDat::open(const std::string& path) {
    open_ = false;
    files_.clear();
    path_ = path;

    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) return false;
    const auto size = static_cast<std::uint64_t>(in.tellg());
    if (size < 16) return false;

    in.seekg(0);
    std::vector<std::uint8_t> header(16);
    in.read(reinterpret_cast<char*>(header.data()), 16);
    if (header[0] != 'P' || header[1] != 'K' || header[2] != 'G' || header[3] != '\n') return false;
    if (be16(&header[4]) != 16 || be16(&header[6]) != 20) return false;

    const std::uint32_t count = be32(&header[8]);
    const std::uint32_t nameSize = be32(&header[12]);
    const std::uint64_t entriesEnd = 16ull + 20ull * count;
    const std::uint64_t namesEnd = entriesEnd + nameSize;
    if (entriesEnd > size || namesEnd > size) return false;

    std::vector<std::uint8_t> entries(20ull * count), names(nameSize);
    in.read(reinterpret_cast<char*>(entries.data()), static_cast<std::streamsize>(entries.size()));
    in.read(reinterpret_cast<char*>(names.data()), static_cast<std::streamsize>(names.size()));
    if (!in) return false;

    for (std::uint32_t i = 0; i < count; ++i) {
        const auto* e = entries.data() + i * 20;
        if (e[4] != 0) return false; // Tachyon currently rejects compressed entries.

        const auto nameOffset = be24(e + 5);
        if (nameOffset >= names.size()) return false;
        std::size_t end = nameOffset;
        while (end < names.size() && names[end] != 0) ++end;
        if (end == names.size()) return false;

        std::string name(reinterpret_cast<const char*>(names.data() + nameOffset), end - nameOffset);
        const auto offset = be32(e + 8);
        const auto compressedSize = be32(e + 12);
        const auto decompressedSize = be32(e + 16);
        if (compressedSize != decompressedSize) return false;
        if (std::uint64_t(offset) + decompressedSize > size) return false;

        files_[std::move(name)] = Entry{offset, decompressedSize};
    }

    open_ = true;
    return true;
}

std::vector<std::uint8_t> FtlDat::readFile(const std::string& name) const {
    if (!open_) return {};
    const auto it = files_.find(name);
    if (it == files_.end()) return {};

    std::ifstream in(path_, std::ios::binary);
    if (!in) return {};
    in.seekg(it->second.offset);

    std::vector<std::uint8_t> out(it->second.length);
    if (!out.empty())
        in.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(out.size()));
    if (!in && !out.empty()) return {};
    return out;
}

bool FtlDat::contains(const std::string& name) const {
    return open_ && files_.find(name) != files_.end();
}

std::vector<std::string> FtlDat::fileNames() const {
    std::vector<std::string> out;
    out.reserve(files_.size());
    for (const auto& [name, _] : files_) out.push_back(name);
    return out;
}

}
