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
    const std::string name = "data/blueprints.xml";
    const std::string payload =
        "<FTL>"
        "<shipBlueprint name=\"PLAYER_SHIP_HARD\" layout=\"kestrel\" shipName=\"The Kestrel\">"
        "<systemList><engines room=\"0\" power=\"1\"/></systemList>"
        "<health amount=\"30\"/><maxPower amount=\"8\"/>"
        "</shipBlueprint>"
        "</FTL>";
    writeFile(path, makeArchive(name, payload));
    wormhole::AssetStore store;
    assert(store.openArchive(path));
    wormhole::BlueprintDatabase database(store);
    assert(database.loadShipBlueprints(name) == 1);
    const auto* ship = database.findShip("PLAYER_SHIP_HARD");
    assert(ship);
    assert(ship->layout == "kestrel");
    assert(ship->name == "The Kestrel");
    std::remove(path.c_str());
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
        137, 80, 78, 71, 13, 10, 26, 10, 0, 0, 0, 13, 73, 72, 68, 82, 0, 0, 0, 1, 0, 0, 0, 1, 8, 6, 0, 0, 0, 31, 21, 196, 137, 0, 0, 0, 13, 73, 68, 65, 84, 120, 156, 99, 248, 207, 192, 240, 31, 0, 5, 0, 1, 255, 137, 153, 61, 29, 0, 0, 0, 0, 73, 69, 78, 68, 174, 66, 96, 130
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
