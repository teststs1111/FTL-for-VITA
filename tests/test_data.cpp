#include "data/bxml.hpp"
#include "data/ftl_dat.hpp"
#include "data/asset_store.hpp"
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
    const std::uint32_t count = 1;
    const std::uint32_t meta = 4 + 4 + static_cast<std::uint32_t>(name.size()) + static_cast<std::uint32_t>(payload.size());
    std::vector<std::uint8_t> data(4 + 4 + meta, 0);
    std::copy(reinterpret_cast<const std::uint8_t*>(&count), reinterpret_cast<const std::uint8_t*>(&count) + 4, data.begin());
    const std::uint32_t offset = 8;
    std::copy(reinterpret_cast<const std::uint8_t*>(&offset), reinterpret_cast<const std::uint8_t*>(&offset) + 4, data.begin() + 4);
    const std::uint32_t len = static_cast<std::uint32_t>(payload.size());
    const std::uint32_t nameLen = static_cast<std::uint32_t>(name.size());
    std::copy(reinterpret_cast<const std::uint8_t*>(&len), reinterpret_cast<const std::uint8_t*>(&len) + 4, data.begin() + 8);
    std::copy(reinterpret_cast<const std::uint8_t*>(&nameLen), reinterpret_cast<const std::uint8_t*>(&nameLen) + 4, data.begin() + 12);
    std::copy(name.begin(), name.end(), data.begin() + 16);
    std::copy(payload.begin(), payload.end(), data.begin() + 16 + name.size());
    return data;
}

static void testVanillaFtlDat() {
    const std::string path = "test_vanilla_ftl.dat";
    writeFile(path, makeVanillaArchive("data/strings.xml", u8"こんにちは")); 
    wormhole::FtlDat archive;
    assert(archive.open(path));
    assert(archive.contains("data/strings.xml"));
    const auto read = archive.readFile("data/strings.xml");
    assert(std::string(read.begin(), read.end()) == u8"こんにちは");
    std::remove(path.c_str());
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
        "<systemList><engines room=\"0\" power=\"2\"/></systemList>"
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

static void testLayoutBlueprint() {
    const std::string layout =
        "X_OFFSET\n10\nY_OFFSET\n20\nHORIZONTAL\n5\nVERTICAL\n4\n"
        "ELLIPSE\n100\n50\n2\n3\n"
        "ROOM\n0\n1\n2\n3\n4\n"
        "ROOM\n1\n5\n6\n2\n2\n"
        "DOOR\n4\n5\n0\n1\n1\n";
    wormhole::LayoutBlueprint parsed;
    assert(wormhole::parseLayoutBlueprint(layout, parsed));
    assert(parsed.xOffset == 10 && parsed.yOffset == 20);
    assert(parsed.rooms.size() == 2);
    assert(parsed.rooms[1].w == 2 && parsed.rooms[1].h == 2);
    assert(parsed.doors.size() == 1 && parsed.doors[0].vertical);
    assert(parsed.doors[0].leftRoom == 0 && parsed.doors[0].rightRoom == 1);
}

static std::vector<std::uint8_t> makeArchive2(
    const std::string& name1, const std::string& payload1,
    const std::string& name2, const std::string& payload2) {
    const std::uint32_t count = 2;
    const std::uint32_t nameSize1 = static_cast<std::uint32_t>(name1.size() + 1);
    const std::uint32_t nameSize2 = static_cast<std::uint32_t>(name2.size() + 1);
    const std::size_t namesStart = 16 + 20 * count;
    const std::uint32_t offset1 = static_cast<std::uint32_t>(namesStart + nameSize1 + nameSize2);
    const std::uint32_t offset2 = offset1 + static_cast<std::uint32_t>(payload1.size());
    std::vector<std::uint8_t> data(offset2 + payload2.size(), 0);
    data[0]='P'; data[1]='K'; data[2]='G'; data[3]='\n';
    data[5]=16;
    data[7]=20;
    data[11]=static_cast<std::uint8_t>(count);
    data[15]=static_cast<std::uint8_t>(nameSize1 + nameSize2);

    auto putEntry = [&](std::size_t entry, std::uint32_t nameOffset,
                         std::uint32_t offset, std::uint32_t size) {
        data[entry+5]=(nameOffset >> 16)&0xff;
        data[entry+6]=(nameOffset >> 8)&0xff;
        data[entry+7]=nameOffset&0xff;
        data[entry+8]=(offset >> 24)&0xff; data[entry+9]=(offset >> 16)&0xff;
        data[entry+10]=(offset >> 8)&0xff; data[entry+11]=offset&0xff;
        data[entry+12]=(size >> 24)&0xff; data[entry+13]=(size >> 16)&0xff;
        data[entry+14]=(size >> 8)&0xff; data[entry+15]=size&0xff;
        data[entry+16]=(size >> 24)&0xff; data[entry+17]=(size >> 16)&0xff;
        data[entry+18]=(size >> 8)&0xff; data[entry+19]=size&0xff;
    };
    const std::uint32_t nameOffset1 = 0;
    const std::uint32_t nameOffset2 = nameSize1;
    putEntry(16, nameOffset1, offset1, static_cast<std::uint32_t>(payload1.size()));
    putEntry(36, nameOffset2, offset2, static_cast<std::uint32_t>(payload2.size()));
    std::copy(name1.begin(), name1.end(), data.begin()+namesStart);
    data[namesStart+name1.size()] = 0;
    std::copy(name2.begin(), name2.end(), data.begin()+namesStart+nameSize1);
    data[namesStart+nameSize1+name2.size()] = 0;
    std::copy(payload1.begin(), payload1.end(), data.begin()+offset1);
    std::copy(payload2.begin(), payload2.end(), data.begin()+offset2);
    return data;
}

static void testShipContent() {
    const std::string path = "test_ship_content.dat";
    const std::string blueprints =
        "<FTL><shipBlueprint name=\"PLAYER_SHIP_HARD\" layout=\"kestrel\" shipName=\"The Kestrel\">"
        "<health amount=\"30\"/><maxPower amount=\"8\"/></shipBlueprint></FTL>";
    const std::string layout =
        "X_OFFSET\n10\nY_OFFSET\n20\nHORIZONTAL\n5\nVERTICAL\n4\n"
        "ELLIPSE\n100\n50\n2\n3\nROOM\n0\n1\n2\n3\n4\n"
        "ROOM\n1\n5\n6\n2\n2\nDOOR\n4\n5\n0\n1\n1\n";
    writeFile(path, makeArchive2("data/blueprints.xml", blueprints, "data/kestrel.txt", layout));
    wormhole::ShipContent content;
    assert(content.open(path));
    assert(content.loadPlayerShip());
    const auto* ship = content.playerShip();
    assert(ship);
    assert(ship->blueprint.name == "The Kestrel");
    assert(ship->blueprint.maxHealth == 30);
    assert(ship->blueprint.startingReactorPower == 8);
    assert(ship->layout.rooms.size() == 2);
    assert(ship->layout.doors.size() == 1);
    assert(ship->layout.ellipseW == 100 && ship->layout.ellipseH == 50);
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
    wormhole::bxml::Node weaponList;
    weaponList.name = "weaponList";
    weaponList.attributes["missiles"] = "7";
    wormhole::bxml::Node weapon;
    weapon.name = "weapon";
    weapon.attributes["name"] = "LASER_BURST_2";
    weaponList.children.push_back(weapon);
    wormhole::bxml::Node droneList;
    droneList.name = "droneList";
    wormhole::bxml::Node drone;
    drone.name = "drone";
    drone.attributes["name"] = "DEFENSE_1";
    droneList.children.push_back(drone);
    ship.attributes["weaponSlots"] = "4";
    ship.attributes["droneSlots"] = "3";
    ship.children={room,system,crew,weaponList,droneList};

    wormhole::ShipBlueprint out;
    assert(wormhole::parseShipBlueprint(ship,out));
    assert(out.id=="PLAYER_SHIP" && out.name=="Kestrel" && out.layout=="kestrel_layout");
    assert(out.rooms.size()==1 && out.rooms[0].id==3 && out.rooms[0].w==2);
    assert(out.systems.size()==1 && out.systems[0].system=="engines" && out.systems[0].level==2);
    assert(out.crew.size()==1 && out.crew[0].race=="human");
    assert(out.weaponSlots == 4 && out.droneSlots == 3);
    assert(out.startingMissiles == 7);
    assert(out.initialWeapons.size() == 1 && out.initialWeapons[0] == "LASER_BURST_2");
    assert(out.initialDrones.size() == 1 && out.initialDrones[0] == "DEFENSE_1");

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


static void testShipRuntime() {
    const std::string blueprintXml =
        "<shipBlueprint name=\"PLAYER_SHIP_HARD\" shipName=\"Kestrel\" layout=\"kestrel\">"
        "<health amount=\"30\"/><maxPower amount=\"8\"/>"
        "<weaponList missiles=\"7\"><weapon name=\"LASER_TEST\"/></weaponList><droneList><drone name=\"COMBAT_TEST\"/><drone name=\"DEFENSE_TEST\"/></droneList>"
        "<systemList><engines room=\"0\" power=\"2\"/><shields room=\"1\" power=\"2\"/><weapons room=\"0\" power=\"1\" start=\"false\"/></systemList>"
        "<crew species=\"human\" name=\"Alice\" room=\"0\"/>"
        "<crew species=\"engi\" name=\"Bob\" room=\"1\"/>"
        "</shipBlueprint>";
    const std::string layout =
        "X_OFFSET\n0\nY_OFFSET\n0\nHORIZONTAL\n5\nVERTICAL\n4\n"
        "ELLIPSE\n100\n50\n0\n0\nROOM\n0\n0\n0\n2\n2\nROOM\n1\n2\n0\n2\n2\nROOM\n2\n4\n0\n2\n2\nDOOR\n2\n0\n0\n1\n0\nDOOR\n4\n0\n1\n2\n0\n";
    const std::string enemyXml =
        "<shipBlueprint name=\"ENEMY_SHIP\" shipName=\"Enemy\" layout=\"kestrel\"><health amount=\"10\"/><maxPower amount=\"8\"/>"
        "<systemList><engines room=\"0\" power=\"2\"/><weapons room=\"2\" power=\"1\"/></systemList><crew species=\"human\" name=\"EnemyAlice\" room=\"0\"/><weaponList missiles=\"2\"><weapon name=\"LASER_TEST\"/></weaponList></shipBlueprint>";
    const std::string weaponXml =
        "<weaponBlueprint name=\"LASER_TEST\" type=\"LASER\" weaponArt=\"laser\" image=\"laser\" "
        "shots=\"2\" damage=\"1\" sysDamage=\"1\" sp=\"0\" missiles=\"1\" speed=\"10\" "
        "power=\"1\" cooldown=\"2.5\" personnelDamage=\"20\"/>"
        "<weaponBlueprint name=\"DRONE_LASER\" type=\"LASER\" weaponArt=\"laser\" image=\"laser\" shots=\"1\" damage=\"1\" sysDamage=\"1\" speed=\"10\" power=\"1\" cooldown=\"1.0\"/>"
        "<droneBlueprint name=\"COMBAT_TEST\"><type>COMBAT</type><power>1</power><speed>10</speed><weaponBlueprint>DRONE_LASER</weaponBlueprint></droneBlueprint>"
        "<droneBlueprint name=\"DEFENSE_TEST\"><type>DEFENSE</type><power>1</power><cooldown>100</cooldown><target>LASERS</target></droneBlueprint>"
        ;
    const std::string fullBlueprints =
        "<FTL>" + blueprintXml + weaponXml + enemyXml + "</FTL>";
    const auto archive = makeArchive({{"data/blueprints.xml", fullBlueprints}, {"data/kestrel.txt", layout}});
    const std::string path = "ship_runtime_test.dat";
    writeFile(path, archive);
    wormhole::ShipContent content;
    assert(content.open(path));
    assert(content.loadPlayerShip());
    wormhole::LoadedShip enemy;
    assert(content.loadShip("ENEMY_SHIP", enemy));
    assert(enemy.blueprint.name == "Enemy");
    assert(enemy.layout.rooms.size() == 3);
    assert(content.loadPlayerShip());
    wormhole::ShipRuntime runtime;
    assert(runtime.load(content));
    assert(runtime.hull == 30 && runtime.reactor == 8);
    assert(runtime.missiles == 7);
    assert(runtime.weapons.size() == 1);
    assert(runtime.drones.size() == 2);
    assert(runtime.setDronePowered(0, true));
    assert(runtime.setDronePowered(1, true));
    runtime.updateDrones(1.0f);
    assert(runtime.drones[0].active);
    assert(runtime.drones[1].active);
    assert(runtime.setDronePowered(0, false));
    assert(runtime.setDronePowered(1, false));
    assert(runtime.maxShieldLayers == 2 && runtime.shieldLayers == 2);
    assert(runtime.setSystemPowered(2, true));
    assert(!runtime.damageShields(1) == false);
    assert(runtime.shieldLayers == 1);
    runtime.updateShields(1.0f);
    assert(runtime.shieldLayers == 1);
    runtime.updateShields(1.0f);
    assert(runtime.shieldLayers == 2);
    assert(runtime.weapons[0].cooldown == 2.5f);
    assert(!runtime.weapons[0].ready);
    runtime.updateWeapons(2.0f);
    assert(!runtime.weapons[0].ready);
    runtime.updateWeapons(0.5f);
    assert(runtime.weapons[0].ready);
    assert(runtime.fireWeapon(0));
    assert(runtime.missiles == 6);
    assert(!runtime.weapons[0].ready);
    assert(!runtime.fireWeapon(0));
    runtime.updateWeapons(2.5f);
    assert(runtime.weapons[0].ready);
    assert(runtime.roomOxygen.size() == 3 && runtime.roomOxygen[0] == 100);
    assert(runtime.setRoomFire(0, true));
    runtime.updateEnvironment(5.0f);
    assert(runtime.roomOxygen[0] == 60 && runtime.roomFire[0]);
    assert(runtime.setDoorOpen(0, true));
    runtime.updateEnvironment(1.0f);
    assert(runtime.roomFire[1]);
    assert(runtime.systems.size() == 3 && runtime.crew.size() == 2);
    assert(runtime.systems[0].type == "engines" && runtime.systems[0].power == 2);
    assert(runtime.systems[0].maxPower == 2);
    assert(runtime.systems[2].type == "weapons" && runtime.systems[2].power == 1);
    assert(!runtime.setSystemPower(0, 3));
    assert(!runtime.setDoorOpen(0, true));
    assert(runtime.setDoorOpen(0, false));
    assert(runtime.crew[1].race == "engi");
    runtime.crew[0].room = 0;
    assert(!runtime.moveCrew(0, 2));
    assert(runtime.setDoorOpen(0, true));
    assert(runtime.setDoorOpen(1, true));
    assert(runtime.moveCrew(0, 2));
    assert(runtime.crew[0].room == 2);
    assert(runtime.moveCrew(0, 1));
    assert(runtime.crew[0].room == 1);
    assert(runtime.extinguishFire(0));
    assert(!runtime.roomFire[1]);
    assert(runtime.moveCrew(0, 0));
    assert(runtime.crew[0].room == 0);
    assert(runtime.crew.size() == 2);
    assert(runtime.damageCrewInRoom(0, 30) == 30);
    assert(runtime.crew[0].health == 70 && runtime.crew[0].alive);
    assert(runtime.healCrew(0, 20) == 20);
    assert(runtime.crew[0].health == 90);
    assert(runtime.healCrew(0, 20) == 10);
    assert(runtime.crew[0].health == 100);
    assert(runtime.damageCrewInRoom(0, 100) == 100);
    assert(runtime.crew[0].health == 0 && !runtime.crew[0].alive);
    assert(runtime.healCrew(0, 100) == 0);
    assert(runtime.setSystemPowered(2, false));
    assert(runtime.usedReactorPower() == 4);
    assert(runtime.availableReactorPower() == 4);
    assert(runtime.damageRoom(0, 5));
    assert(runtime.hull == 25 && runtime.roomDamage[0] == 5);
    assert(runtime.repairRoom(0, 3));
    assert(runtime.hull == 28 && runtime.roomDamage[0] == 2);
    assert(runtime.setSystemPowered(2, false));
    assert(runtime.setSystemPowered(0, false));
    assert(runtime.usedReactorPower() == 2);
    assert(runtime.setSystemPowered(0, true));

    wormhole::CombatRuntime combat;
    wormhole::LoadedShip enemyForCombat;
    assert(content.loadShip("ENEMY_SHIP", enemyForCombat));
    assert(combat.load(content, enemyForCombat));
    assert(combat.setTargetRoom(0));
    assert(combat.player.drones.size() == 2);
    assert(combat.player.drones[0].weaponDamage == 1);
    assert(combat.player.drones[0].weaponSystemDamage == 1);
    assert(combat.player.drones[0].weaponSpeed == 10);
    assert(combat.player.drones[0].weaponCooldown == 1.0f);
    assert(combat.player.setDronePowered(0, true));
    assert(combat.player.setDronePowered(1, true));
    for (auto& weapon : combat.enemy.weapons) {
        weapon.ready = false;
        weapon.charge = 0.0f;
    }
    combat.enemy.drones.clear();
    combat.update(0.9f);
    assert(combat.player.drones[0].active == false);
    combat.enemy.shieldLayers = 0;
    const int droneHullBefore = combat.enemy.hull;
    combat.update(0.1f);
    assert(combat.pendingShotCount() >= 1);
    combat.update(0.14f);
    assert(combat.enemy.hull == droneHullBefore);
    assert(combat.pendingShotCount() == 1);
    combat.update(0.02f);
    wormhole::CombatResult droneImpact;
    assert(combat.consumeImpactResult(droneImpact));
    assert(droneImpact.fired);
    assert(droneImpact.shotsFired == 1);
    assert(droneImpact.hullDamage == 1);
    assert(combat.enemy.hull == droneHullBefore - 1);
    assert(combat.player.setDronePowered(0, false));
    assert(combat.player.setDronePowered(1, false));
    assert(combat.setTargetRoom(0));
    assert(combat.load(content, enemyForCombat));
    assert(combat.player.setDronePowered(1, true));
    combat.enemy.setSystemPowered(1, true);
    combat.enemy.updateWeapons(2.5f);
    combat.player.shieldLayers = 0;
    const int defenseHullBefore = combat.player.hull;
    combat.update(0.1f);
    // The defense drone intercepts the incoming laser on this update and immediately spends its charge.\n    assert(combat.player.drones[1].active == false);
    combat.update(0.01f);
    assert(combat.player.hull == defenseHullBefore);
    assert(combat.pendingShotCount() == 0);
    assert(combat.setTargetRoom(0));
    assert(combat.player.setSystemPowered(2, true));
    combat.player.updateWeapons(2.5f);
    assert(combat.player.weapons[0].ready);
    assert(combat.player.weapons[0].speed == 10);
    const int enemyHullBefore = combat.enemy.hull;
    auto combatResult = combat.fireSelectedWeapon();
    assert(combatResult.fired);
    assert(combatResult.shotsFired == 2);
    assert(combatResult.hullDamage == 0);
    assert(combat.enemy.hull == enemyHullBefore);
    assert(combat.pendingShotCount() == 2);
    combat.update(0.25f);
    assert(combat.enemy.hull == enemyHullBefore - 1);
    combat.update(0.03f);
    assert(combat.enemy.hull == enemyHullBefore - 2);
    assert(combat.enemy.systems[0].damage == 2);
    assert(combat.enemy.crew[0].health == 60);
    wormhole::CombatResult impact1;
    wormhole::CombatResult impact2;
    assert(combat.consumeImpactResult(impact1));
    assert(impact1.hullDamage == 1);
    assert(impact1.personnelDamage == 20);
    assert(combat.consumeImpactResult(impact2));
    assert(impact2.hullDamage == 1);
    assert(impact2.personnelDamage == 20);
    assert(!combat.consumeImpactResult(impact2));
    assert(combat.enemy.systems[0].power == 0);
    assert(!combat.enemy.systems[0].powered);
    assert(combat.player.missiles == 6);
    assert(!combat.player.weapons[0].ready);
    combat.player.updateWeapons(2.2f);
    assert(!combat.player.weapons[0].ready);
    combat.player.updateWeapons(0.1f);
    assert(combat.player.weapons[0].ready);

    assert(combat.load(content, enemyForCombat));
    assert(combat.setTargetRoom(0));
    assert(combat.player.setSystemPowered(2, true));
    combat.player.weapons[0].shieldPiercing = 0;
    combat.player.updateWeapons(2.5f);
    combat.enemy.shieldLayers = 1;
    const int shieldedHull = combat.enemy.hull;
    auto shieldResult = combat.fireSelectedWeapon();
    assert(shieldResult.shieldsAbsorbed == 0);
    assert(shieldResult.hullDamage == 0);
    assert(combat.enemy.hull == shieldedHull);
    combat.update(0.25f);
    assert(combat.enemy.shieldLayers == 0);
    assert(combat.enemy.hull == shieldedHull - 1);
    assert(combat.pendingShotCount() == 1);

    assert(combat.load(content, enemyForCombat));
    assert(combat.setTargetRoom(0));
    assert(combat.player.setSystemPowered(2, true));
    combat.player.weapons[0].shieldPiercing = 1;
    combat.player.updateWeapons(2.5f);
    combat.enemy.shieldLayers = 1;
    const int piercedHull = combat.enemy.hull;
    auto piercingResult = combat.fireSelectedWeapon();
    assert(piercingResult.shieldsAbsorbed == 0);
    assert(piercingResult.hullDamage == 0);
    assert(combat.enemy.hull == piercedHull);
    combat.update(0.25f);
    assert(combat.enemy.hull == piercedHull - 1);
    combat.update(0.03f);
    assert(combat.enemy.hull == piercedHull - 2);

    assert(combat.load(content, enemyForCombat));
    assert(combat.enemyTargetRoom == 0);
    assert(combat.enemy.setSystemPowered(1, true));
    combat.enemy.updateWeapons(2.5f);
    combat.player.shieldLayers = 0;
    const int playerHullBeforeEnemyShot = combat.player.hull;
    combat.update(0.1f);
    assert(combat.player.hull == playerHullBeforeEnemyShot);
    assert(combat.pendingShotCount() == 2);
    combat.update(0.15f);
    assert(combat.player.hull == playerHullBeforeEnemyShot - 1);
    combat.update(0.03f);
    assert(combat.player.hull == playerHullBeforeEnemyShot - 2);
    assert(!combat.enemy.weapons[0].ready);
    assert(combat.outcome == wormhole::CombatOutcome::Ongoing);

    assert(combat.load(content, enemyForCombat));
    assert(combat.enemy.setSystemPowered(1, true));
    combat.enemy.updateWeapons(2.5f);
    combat.player.shieldLayers = 0;
    combat.player.hull = 1;
    combat.update(0.25f);
    assert(combat.outcome == wormhole::CombatOutcome::PlayerDestroyed);

    assert(combat.load(content, enemyForCombat));
    combat.enemy.hull = 1;
    assert(combat.setTargetRoom(0));
    assert(combat.player.setSystemPowered(2, true));
    combat.player.updateWeapons(2.5f);
    auto killResult = combat.fireSelectedWeapon();
    assert(!killResult.targetDestroyed);
    assert(combat.outcome == wormhole::CombatOutcome::Ongoing);
    combat.update(0.25f);
    assert(combat.outcome == wormhole::CombatOutcome::EnemyDestroyed);

    runtime.reset();
    assert(!runtime.valid && runtime.systems.empty() && runtime.crew.empty());
    assert(runtime.shieldLayers == 0 && runtime.maxShieldLayers == 0);
    std::remove(path.c_str());
}

int main() {
    testBxml();
    testFtlDat();
    testVanillaFtlDat();
    testAssetStore();
    testLayoutBlueprint();
    testShipBlueprint();
    testShipContent();
    testBlueprintDatabase();
    testPng();
    testShipRuntime();
    return 0;
}
