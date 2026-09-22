#include "data/bxml.hpp"
#include "data/ftl_dat.hpp"
#include <cassert>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

static void writeFile(const std::string& path, const std::vector<std::uint8_t>& data) {
    std::ofstream out(path, std::ios::binary);
    assert(out);
    out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
}

static void testBxml() {
    // Root element "root", one attribute key="value", one text node "hello".
    // Strings are document-local: id=0 means a new string, id=n references string n.
    const std::vector<std::uint8_t> bxml = {
        0x00, 0x04, 'r','o','o','t',
        0x01,
        0x00, 0x03, 'k','e','y',
        0x00, 0x05, 'v','a','l','u','e',
        0x00, 0x05, 'h','e','l','l','o',
        0x02
    };
    const auto node = wormhole::bxml::read(bxml);
    assert(node.name == "root");
    assert(node.attributes.at("key") == "value");
    assert(node.text == "hello");
    assert(node.children.empty());
}

static void testFtlDat() {
    const std::string path = "test_ftl.dat";
    const std::string name = "hello.txt";
    const std::string payload = "hello";
    const std::uint32_t count = 1;
    const std::uint32_t nameSize = static_cast<std::uint32_t>(name.size() + 1);

    std::vector<std::uint8_t> data(16 + 20 + nameSize + payload.size(), 0);
    data[0]='P'; data[1]='K'; data[2]='G'; data[3]='\\n';
    data[4]=0; data[5]=16; // header size
    data[6]=0; data[7]=20; // entry size
    data[8]=(count >> 24) & 0xff; data[9]=(count >> 16) & 0xff;
    data[10]=(count >> 8) & 0xff; data[11]=count & 0xff;
    data[12]=(nameSize >> 24) & 0xff; data[13]=(nameSize >> 16) & 0xff;
    data[14]=(nameSize >> 8) & 0xff; data[15]=nameSize & 0xff;

    const std::size_t entry=16;
    const std::uint32_t offset=16+20+nameSize;
    data[entry+5]=0; data[entry+6]=0; data[entry+7]=0;
    data[entry+8]=(offset >> 24)&0xff; data[entry+9]=(offset >> 16)&0xff;
    data[entry+10]=(offset >> 8)&0xff; data[entry+11]=offset&0xff;
    const std::uint32_t size=static_cast<std::uint32_t>(payload.size());
    data[entry+12]=(size >> 24)&0xff; data[entry+13]=(size >> 16)&0xff;
    data[entry+14]=(size >> 8)&0xff; data[entry+15]=size&0xff;
    data[entry+16]=(size >> 24)&0xff; data[entry+17]=(size >> 16)&0xff;
    data[entry+18]=(size >> 8)&0xff; data[entry+19]=size&0xff;

    std::copy(name.begin(), name.end(), data.begin()+36);
    data[36+name.size()]=0;
    std::copy(payload.begin(), payload.end(), data.begin()+offset);

    writeFile(path, data);

    wormhole::FtlDat archive;
    assert(archive.open(path));
    assert(archive.contains(name));
    const auto read = archive.readFile(name);
    assert(std::string(read.begin(), read.end()) == payload);
    assert(!archive.contains("missing"));

    std::remove(path.c_str());
}

int main() {
    testBxml();
    testFtlDat();
    return 0;
}
