#include "data/bxml.hpp"
#include "data/ftl_dat.hpp"
#include "data/asset_store.hpp"
#include "data/event_database.hpp"
#include "data/blueprint_database.hpp"
#include "data/ship_blueprint.hpp"
#include "data/ship_content.hpp"
#include "game/ship_runtime.hpp"
#include "game/combat_runtime.hpp"
#include "render/png_loader.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

using namespace wormhole;

static std::vector<std::uint8_t> makeArchive(const std::vector<std::pair<std::string, std::string>>& files) {
    const std::size_t count = files.size();
    std::size_t names = 0, payloads = 0;
    for (const auto& f : files) { names += f.first.size() + 1; payloads += f.second.size(); }
    std::vector<std::uint8_t> data(16 + 20 * count + names + payloads, 0);
    data[0]='P'; data[1]='K'; data[2]='G'; data[3]='\n';
    data[5]=16; data[7]=20;
    data[8]=static_cast<std::uint8_t>(count >> 24);
    data[9]=static_cast<std::uint8_t>(count >> 16);
    data[10]=static_cast<std::uint8_t>(count >> 8);
    data[11]=static_cast<std::uint8_t>(count);
    data[12]=static_cast<std::uint8_t>(names >> 24);
    data[13]=static_cast<std::uint8_t>(names >> 16);
    data[14]=static_cast<std::uint8_t>(names >> 8);
    data[15]=static_cast<std::uint8_t>(names);
    std::size_t nameOffset = 16 + 20 * count;
    std::size_t payloadOffset = nameOffset + names;
    for (std::size_t i=0; i<count; ++i) {
        const auto& f = files[i];
        const std::size_t e = 16 + 20 * i;
        const std::uint32_t no = static_cast<std::uint32_t>(nameOffset - (16 + 20 * count));
        const std::uint32_t po = static_cast<std::uint32_t>(payloadOffset);
        data[e+5]=static_cast<std::uint8_t>(no >> 16); data[e+6]=static_cast<std::uint8_t>(no >> 8); data[e+7]=static_cast<std::uint8_t>(no);
        data[e+8]=static_cast<std::uint8_t>(po >> 24); data[e+9]=static_cast<std::uint8_t>(po >> 16); data[e+10]=static_cast<std::uint8_t>(po >> 8); data[e+11]=static_cast<std::uint8_t>(po);
        const std::uint32_t size = static_cast<std::uint32_t>(f.second.size());
        data[e+12]=static_cast<std::uint8_t>(size >> 24); data[e+13]=static_cast<std::uint8_t>(size >> 16); data[e+14]=static_cast<std::uint8_t>(size >> 8); data[e+15]=static_cast<std::uint8_t>(size);
        data[e+16]=data[e+12]; data[e+17]=data[e+13]; data[e+18]=data[e+14]; data[e+19]=data[e+15];
        std::copy(f.first.begin(), f.first.end(), data.begin()+nameOffset); data[nameOffset+f.first.size()]=0;
        nameOffset += f.first.size()+1;
        std::copy(f.second.begin(), f.second.end(), data.begin()+payloadOffset); payloadOffset += f.second.size();
    }
    return data;
}

static void writeFile(const std::string& path, const std::vector<std::uint8_t>& data) {
    std::ofstream out(path, std::ios::binary);
    assert(out);
    out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
}

static std::vector<std::uint8_t> makeArchive(const std::string& name, const std::string& payload) {
    const std::uint32_t count = 1;
    const std::uint32_t nameSize = static_cast<std::uint32_t>(name.size() + 1);
    std::vector<std::uint8_t> data(16 + 20 + nameSize + payload.size(), 0);
    data[0]='P'; data[1]='K'; data[2]='G'; data[3]='\n';
    data[5]=16; data[7]=20;
    data[11]=static_cast<std::uint8_t>(count);
    data[15]=static_cast<std::uint8_t>(nameSize);
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
    return data;
}

static void testBxml() {
    const std::string xml =
        "<?xml version=\"1.0\"?>"
        "<root key=\"value &amp; more\">"
        "hello &lt;world&gt;"
        "<child id=\"7\">text</child>"
        "<empty/>"
        "</root>";
    const std::vector<std::uint8_t> data(xml.begin(), xml.end());
    const auto node = wormhole::bxml::read(data);
    assert(node.name == "root");
    assert(node.attributes.at("key") == "value & more");
    assert(node.text == "hello <world>");
    assert(node.children.size() == 2);
    assert(node.children[0].name == "child");
    assert(node.children[0].attributes.at("id") == "7");
    assert(node.children[0].text == "text");
    assert(node.children[1].name == "empty");
}

static std::vector<std::uint8_t> makeVanillaArchive(const std::string& name, const std::string& payload) {
    const std::size_t nameSize = name.size() + 1;
    const std::size_t namesOffset = 16 + 20;
    const std::size_t payloadOffset = namesOffset + nameSize;
    std::vector<std::uint8_t> data(payloadOffset + payload.size(), 0);
    data[0]='P'; data[1]='K'; data[2]='G'; data[3]='\\n';
    data[5]=16; data[7]=20; data[11]=1;
    data[12]=static_cast<std::uint8_t>(nameSize >> 24);
    data[13]=static_cast<std::uint8_t>(nameSize >> 16);
    data[14]=static_cast<std::uint8_t>(nameSize >> 8);
    data[15]=static_cast<std::uint8_t>(nameSize);
    const std::size_t entry=16;
    const std::uint32_t offset=static_cast<std::uint32_t>(payloadOffset);
    data[entry+8]=(offset >> 24)&0xff; data[entry+9]=(offset >> 16)&0xff;
    data[entry+10]=(offset >> 8)&0xff; data[entry+11]=offset&0xff;
    const std::uint32_t size=static_cast<std::uint32_t>(payload.size());
    data[entry+12]=(size >> 24)&0xff; data[entry+13]=(size >> 16)&0xff;
    data[entry+14]=(size >> 8)&0xff; data[entry+15]=size&0xff;
    data[entry+16]=data[entry+12]; data[entry+17]=data[entry+13];
    data[entry+18]=data[entry+14]; data[entry+19]=data[entry+15];
    std::copy(name.begin(), name.end(), data.begin()+namesOffset);
    data[namesOffset+name.size()]=0;
    std::copy(payload.begin(), payload.end(), data.begin()+payloadOffset);
    return data;
}

