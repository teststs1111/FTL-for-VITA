#include "data/ftl_dat.hpp"
#include <fstream>
#include <limits>
#include <zlib.h>

namespace wormhole {
namespace {
std::uint16_t be16(const std::uint8_t* p) {
    return (std::uint16_t(p[0]) << 8) | p[1];
}
std::uint32_t be32(const std::uint8_t* p) {
    return (std::uint32_t(p[0]) << 24) | (std::uint32_t(p[1]) << 16) |
           (std::uint32_t(p[2]) << 8) | p[3];
}
constexpr std::uint32_t PKGF_DEFLATED = 1u << 24;
}

bool FtlDat::open(const std::string& path) {
    open_ = false;
    files_.clear();
    paths_.clear();

    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) return false;
    const auto size = static_cast<std::uint64_t>(in.tellg());
    if (size < 16) return false;

    std::uint8_t header[16]{};
    in.seekg(0);
    in.read(reinterpret_cast<char*>(header), sizeof(header));
    if (!in) return false;

    // FTL 1.6+ uses SIL's PKG container for ftl.dat.
    if (header[0] != 'P' || header[1] != 'K' ||
        header[2] != 'G' || header[3] != '\n' ||
        be16(header + 4) != 16 || be16(header + 6) != 20) {
        return false;
    }

    const std::uint32_t count = be32(header + 8);
    const std::uint32_t nameSize = be32(header + 12);
    const std::uint64_t entriesSize = 20ull * count;
    const std::uint64_t entriesEnd = 16ull + entriesSize;
    const std::uint64_t namesEnd = entriesEnd + nameSize;
    if (count == 0 || entriesEnd > size || namesEnd > size ||
        entriesSize > static_cast<std::uint64_t>(
            std::numeric_limits<std::streamsize>::max())) {
        return false;
    }

    std::vector<std::uint8_t> entries(static_cast<std::size_t>(entriesSize));
    std::vector<std::uint8_t> names(nameSize);
    in.seekg(16);
    in.read(reinterpret_cast<char*>(entries.data()),
            static_cast<std::streamsize>(entries.size()));
    in.read(reinterpret_cast<char*>(names.data()),
            static_cast<std::streamsize>(names.size()));
    if (!in) return false;

    for (std::uint32_t i = 0; i < count; ++i) {
        const auto* e = entries.data() + static_cast<std::size_t>(i) * 20;
        const std::uint32_t nameofsFlags = be32(e + 4);
        const std::uint32_t nameOffset = nameofsFlags & 0x00FFFFFFu;
        const std::uint32_t offset = be32(e + 8);
        const std::uint32_t dataLength = be32(e + 12);
        const std::uint32_t fileSize = be32(e + 16);

        if (nameOffset >= names.size()) return false;
        std::size_t end = nameOffset;
        while (end < names.size() && names[end] != 0) ++end;
        if (end == names.size()) return false;

        const std::uint64_t dataEnd =
            static_cast<std::uint64_t>(offset) + dataLength;
        if (dataEnd > size) return false;
        if ((nameofsFlags & PKGF_DEFLATED) == 0 && dataLength != fileSize)
            return false;

        std::string name(
            reinterpret_cast<const char*>(names.data() + nameOffset),
            end - nameOffset);
        if (name.empty()) return false;

        files_[std::move(name)] =
            Entry{offset, dataLength, fileSize,
                  (nameofsFlags & PKGF_DEFLATED) != 0, 0};
    }

    if (files_.empty()) return false;
    open_ = true;
    paths_.push_back(path);
    return true;
}

std::vector<std::uint8_t> FtlDat::readFile(const std::string& name) const {
    if (!open_) return {};
    const auto it = files_.find(name);
    if (it == files_.end() || it->second.archiveIndex >= paths_.size())
        return {};

    const Entry& entry = it->second;
    std::ifstream in(paths_[entry.archiveIndex], std::ios::binary);
    if (!in) return {};
    in.seekg(entry.offset);

    std::vector<std::uint8_t> compressed(entry.length);
    if (!compressed.empty())
        in.read(reinterpret_cast<char*>(compressed.data()),
                static_cast<std::streamsize>(compressed.size()));
    if (!in && !compressed.empty()) return {};

    if (!entry.compressed) return compressed;
    if (entry.uncompressedLength == 0) return {};

    std::vector<std::uint8_t> out(entry.uncompressedLength);
    uLongf outSize = static_cast<uLongf>(out.size());
    const int result = ::uncompress(
        out.data(), &outSize, compressed.data(),
        static_cast<uLong>(compressed.size()));
    if (result != Z_OK || outSize != out.size()) return {};
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
            if (it != pack.files_.end()) {
                Entry entry = it->second;
                entry.archiveIndex = archiveIndex;
                files_[name] = entry;
            }
        }
    }

    open_ = !files_.empty();
    return open_;
}

}  // namespace wormhole
