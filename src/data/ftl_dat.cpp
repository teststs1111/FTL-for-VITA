#include "data/ftl_dat.hpp"
#include <algorithm>
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
    paths_.clear();

    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) return false;
    const auto size = static_cast<std::uint64_t>(in.tellg());
    if (size < 8) return false;

    // Tachyon's reconstructed PKG container is kept for the existing prototype.
    in.seekg(0);
    std::uint8_t magic[16]{};
    in.read(reinterpret_cast<char*>(magic), static_cast<std::streamsize>(std::min<std::uint64_t>(16, size)));
    if (size >= 16 && magic[0] == 'P' && magic[1] == 'K' && magic[2] == 'G' && magic[3] == '\n' &&
        be16(&magic[4]) == 16 && be16(&magic[6]) == 20) {
        const std::uint32_t count = be32(&magic[8]);
        const std::uint32_t nameSize = be32(&magic[12]);
        const std::uint64_t entriesEnd = 16ull + 20ull * count;
        const std::uint64_t namesEnd = entriesEnd + nameSize;
        if (entriesEnd > size || namesEnd > size) return false;
        std::vector<std::uint8_t> entries(20ull * count), names(nameSize);
        in.seekg(16);
        in.read(reinterpret_cast<char*>(entries.data()), static_cast<std::streamsize>(entries.size()));
        in.read(reinterpret_cast<char*>(names.data()), static_cast<std::streamsize>(names.size()));
        if (!in) return false;
        for (std::uint32_t i = 0; i < count; ++i) {
            const auto* e = entries.data() + i * 20;
            if (e[4] != 0) return false;
            const auto nameOffset = be24(e + 5);
            if (nameOffset >= names.size()) return false;
            std::size_t end = nameOffset;
            while (end < names.size() && names[end] != 0) ++end;
            if (end == names.size()) return false;
            std::string name(reinterpret_cast<const char*>(names.data() + nameOffset), end - nameOffset);
            const auto offset = be32(e + 8);
            const auto compressedSize = be32(e + 12);
            const auto decompressedSize = be32(e + 16);
            if (compressedSize != decompressedSize || std::uint64_t(offset) + decompressedSize > size) return false;
            files_[std::move(name)] = Entry{offset, decompressedSize, 0};
        }
        open_ = true;
        paths_.push_back(path);
        return true;
    }

    // Vanilla FTL 1.6+ ftl.dat: little-endian file-slot table with UTF-8 names.
    in.seekg(0);
    std::uint32_t countBytes[1]{};
    in.read(reinterpret_cast<char*>(countBytes), 4);
    const std::uint32_t count = countBytes[0];
    if (!in || count == 0 || count > 100000) return false;
    if (8ull * count + 4ull > size) return false;
    std::vector<std::uint32_t> offsets(count);
    in.seekg(4);
    in.read(reinterpret_cast<char*>(offsets.data()), static_cast<std::streamsize>(4ull * count));
    if (!in) return false;
    for (const auto offset : offsets) {
        if (offset == 0) continue;
        if (std::uint64_t(offset) + 8ull > size) return false;
        in.seekg(offset);
        std::uint32_t len = 0, nameLen = 0;
        in.read(reinterpret_cast<char*>(&len), 4);
        in.read(reinterpret_cast<char*>(&nameLen), 4);
        if (!in || nameLen == 0 || std::uint64_t(offset) + 8ull + nameLen + len > size) return false;
        std::string name(nameLen, '\0');
        in.read(name.data(), static_cast<std::streamsize>(nameLen));
        if (!in) return false;
        const auto bodyOffset = static_cast<std::uint32_t>(offset + 8ull + nameLen);
        files_[std::move(name)] = Entry{bodyOffset, len, 0};
    }
    open_ = !files_.empty();
    if (open_) paths_.push_back(path);
    return open_;
}

std::vector<std::uint8_t> FtlDat::readFile(const std::string& name) const {
    if (!open_) return {};
    const auto it = files_.find(name);
    if (it == files_.end()) return {};

    if (it->second.archiveIndex >= paths_.size()) return {};\n    std::ifstream in(paths_[it->second.archiveIndex], std::ios::binary);
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



namespace wormhole {
bool FtlDat::openArchives(const std::vector<std::string>& paths) {
    open_ = false;
    files_.clear();
    paths_.clear();
    for (const auto& path : paths) {
        FtlDat pack;
        if (!pack.open(path)) continue;
        const auto archiveIndex = static_cast<std::uint32_t>(paths_.size());
        paths_.push_back(path);
        for (const auto& name : pack.fileNames()) {
            const auto it = pack.files_.find(name);
            if (it != pack.files_.end())
                files_[name] = Entry{it->second.offset, it->second.length, archiveIndex};
        }
    }
    open_ = !files_.empty();
    return open_;
}
}
