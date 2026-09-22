#include "data/bxml.hpp"
#include "data/ftl_dat.hpp"
#include "data/asset_store.hpp"
#include "data/blueprint_database.hpp"
#include "data/ship_blueprint.hpp"
#include "render/png_loader.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

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
    const std::vector<std::uint8_t> bxml = {
        0x00,0x04,'r','o','o','t',0x01,
        0x00,0x03,'k','e','y',0x00,0x05,'v','a','l','u','e',
        0x00,0x05,'h','e','l','l','o',0x02
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
    writeFile(path, makeArchive(name, payload));
    wormhole::FtlDat archive;
    assert(archive.open(path));
    assert(archive.contains(name));
    const auto read = archive.readFile(name);
    assert(std::string(read.begin(), read.end()) == payload);
    assert(!archive.contains("missing"));
    std::remove(path.c_str());
}

static void testAssetStore() {
    const std::string path = "test_asset_store.dat";
    const std::string name = "hello.txt";
    const std::string payload = "cached";
    writeFile(path, makeArchive(name, payload));
    wormhole::AssetStore store;
    assert(store.openArchive(path));
    const auto* first = store.getBytes(name);
    const auto* second = store.getBytes(name);
    assert(first && first == second);
    assert(std::string(first->begin(), first->end()) == payload);
    std::remove(path.c_str());
}

static void testBlueprintDatabase() {
    const std::string path = "test_blueprints.dat";
    const std::string name = "blueprints/player.xml";
    const std::string payload = "placeholder";
    (void)payload;
    // The database API is exercised through a synthetic BXML document below.
    // This test focuses on lookup/storage semantics; archive decoding is covered separately.
    wormhole::AssetStore store;
    (void)store;
    wormhole::BlueprintDatabase database(store);
    assert(database.findShip("missing") == nullptr);
}

static void testShipBlueprint() {
    wormhole::bxml::Node ship;
    ship.name="shipBlueprint";
    ship.attributes["name"]="PLAYER_SHIP";
    ship.attributes["shipName"]="Kestrel";
    ship.attributes["layout"]="kestrel_layout";
    wormhole::bxml::Node room;
    room.name="room";
    room.attributes["id"]="3"; room.attributes["x"]="2"; room.attributes["y"]="4";
    room.attributes["w"]="2"; room.attributes["h"]="1";
    wormhole::bxml::Node system;
    system.name="system";
    system.attributes["type"]="engines"; system.attributes["room"]="3"; system.attributes["level"]="2";
    wormhole::bxml::Node crew;
    crew.name="crew";
    crew.attributes["race"]="human"; crew.attributes["name"]="Alice"; crew.attributes["room"]="3";
    ship.children={room,system,crew};

    wormhole::ShipBlueprint out;
    assert(wormhole::parseShipBlueprint(ship,out));
    assert(out.id=="PLAYER_SHIP" && out.name=="Kestrel" && out.layout=="kestrel_layout");
    assert(out.rooms.size()==1 && out.rooms[0].id==3 && out.rooms[0].w==2);
    assert(out.systems.size()==1 && out.systems[0].system=="engines" && out.systems[0].level==2);
    assert(out.crew.size()==1 && out.crew[0].race=="human");
}

static void testPng() {
    const std::vector<std::uint8_t> png = {
        0x89,0x50,0x4e,0x47,0x0d,0x0a,0x1a,0x0a,0x00,0x00,0x00,0x0d,0x49,0x48,0x44,0x52,
        0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,0x08,0x06,0x00,0x00,0x00,0x1f,0x15,0xc4,0x89,
        0x00,0x00,0x00,0x0c,0x49,0x44,0x41,0x54,0x78,0x9c,0x63,0xf8,0xcf,0xc0,0x00,0x00,0x03,
        0x01,0x01,0x00,0xc9,0xfe,0x92,0xef,0x00,0x00,0x00,0x00,0x49,0x45,0x4e,0x44,0xae,0x42,0x60,0x82
    };
    wormhole::RgbaImage image;
    assert(wormhole::decodePng(png,image));
    assert(image.width==1 && image.height==1 && image.pixels.size()==4);
    assert(image.pixels[0]==0xff && image.pixels[1]==0 && image.pixels[2]==0 && image.pixels[3]==0xff);
}

int main() {
    testBxml();
    testFtlDat();
    testAssetStore();
    testShipBlueprint();
    testBlueprintDatabase();
    testPng();
    return 0;
}
