#include "data/ftl_dat.hpp"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>
#include <zlib.h>

namespace {
void be16(std::ofstream& out, std::uint16_t v) {
    const char b[] = {char(v >> 8), char(v)};
    out.write(b, sizeof(b));
}
void be32(std::ofstream& out, std::uint32_t v) {
    const char b[] = {
        char(v >> 24), char(v >> 16), char(v >> 8), char(v)
    };
    out.write(b, sizeof(b));
}
}

int main() {
    const std::string path = "test_ftl_pkg.dat";
    const std::string rawName = "data/raw.txt";
    const std::string compressedName = "data/compressed.txt";
    const std::string raw = "canonical raw payload";
    const std::string original = "canonical compressed payload";
    const std::string names = rawName + '\0' + compressedName + '\0';

    uLongf compressedSize = compressBound(original.size());
    std::vector<std::uint8_t> compressed(compressedSize);
    assert(compress(compressed.data(), &compressedSize,
                    reinterpret_cast<const Bytef*>(original.data()),
                    original.size()) == Z_OK);
    compressed.resize(compressedSize);

    const std::uint32_t rawOffset = 16 + 20 * 2 + names.size();
    const std::uint32_t compressedOffset = rawOffset + raw.size();

    std::ofstream out(path, std::ios::binary);
    assert(out);
    out.write("PKG\n", 4);
    be16(out, 16);
    be16(out, 20);
    be32(out, 2);
    be32(out, names.size());

    // hash values are not relevant to FtlDat's linear index loading.
    be32(out, 0);
    be32(out, 0);
    be32(out, rawOffset);
    be32(out, raw.size());
    be32(out, raw.size());

    be32(out, 0);
    be32(out, 0x01000000u);
    be32(out, compressedOffset);
    be32(out, compressed.size());
    be32(out, original.size());

    out.write(names.data(), names.size());
    out.write(raw.data(), raw.size());
    out.write(reinterpret_cast<const char*>(compressed.data()),
              compressed.size());
    out.close();

    wormhole::FtlDat archive;
    assert(archive.open(path));
    assert(archive.fileNames().size() == 2);
    assert(archive.readFile(rawName) ==
           std::vector<std::uint8_t>(raw.begin(), raw.end()));
    assert(archive.readFile(compressedName) ==
           std::vector<std::uint8_t>(original.begin(), original.end()));

    std::remove(path.c_str());
    return 0;
}
