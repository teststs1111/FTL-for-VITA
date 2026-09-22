#include "data/ship_blueprint.hpp"
#include <cstdlib>
#include <algorithm>
#include <sstream>
#include <stdexcept>

namespace wormhole {
namespace {

int integer(const bxml::Node& node, const char* key, int fallback = 0) {
    const auto it = node.attributes.find(key);
    if (it == node.attributes.end()) return fallback;
    char* end = nullptr;
    const long value = std::strtol(it->second.c_str(), &end, 10);
    return (end == it->second.c_str()) ? fallback : static_cast<int>(value);
}

std::string attribute(const bxml::Node& node, const char* key) {
    const auto it = node.attributes.find(key);
    return it == node.attributes.end() ? std::string{} : it->second;
}

const bxml::Node* child(const bxml::Node& node, const char* name) {
    for (const auto& c : node.children)
        if (c.name == name) return &c;
    return nullptr;
}

void collectRooms(const bxml::Node& node, std::vector<RoomBlueprint>& rooms) {
    if (node.name == "room") {
        RoomBlueprint room;
        room.id = integer(node, "id", -1);
        room.x = integer(node, "x");
        room.y = integer(node, "y");
        room.w = integer(node, "w", integer(node, "width", 1));
        room.h = integer(node, "h", integer(node, "height", 1));
        rooms.push_back(room);
    }
    for (const auto& c : node.children) collectRooms(c, rooms);
}

void collectSystems(const bxml::Node& node, std::vector<SystemSlotBlueprint>& systems) {
    if (node.name == "system") {
        SystemSlotBlueprint system;
        system.system = attribute(node, "type");
        if (system.system.empty()) system.system = attribute(node, "name");
        system.room = integer(node, "room", -1);
        system.level = integer(node, "level", integer(node, "power"));
        systems.push_back(std::move(system));
    } else if (node.name == "systemList") {
        // Tachyon's FTL blueprints use the element name itself for the
        // system type, e.g. <engines room="0" power="1"/>.
        for (const auto& c : node.children) {
            if (c.name == "system") {
                collectSystems(c, systems);
                continue;
            }
            if (c.name.empty()) continue;
            SystemSlotBlueprint system;
            system.system = c.name;
            system.room = integer(c, "room", -1);
            system.startingPower = integer(c, "power", integer(c, "level"));
            system.level = system.startingPower;
            system.maxPower = integer(c, "max", system.startingPower);
            system.availableByDefault = attribute(c, "start") != "false";
            systems.push_back(std::move(system));
        }
        return;
    }
    for (const auto& c : node.children) collectSystems(c, systems);
}

void collectCrew(const bxml::Node& node, std::vector<CrewBlueprint>& crew) {
    if (node.name == "crew") {
        CrewBlueprint member;
        member.race = attribute(node, "species");
        if (member.race.empty()) member.race = attribute(node, "race");
        member.name = attribute(node, "name");
        member.room = integer(node, "room", -1);
        crew.push_back(std::move(member));
    } else if (node.name == "crewCount") {
        const int amount = std::max(0, integer(node, "amount", 0));
        const std::string race = attribute(node, "class").empty() ? "human" : attribute(node, "class");
        for (int i = 0; i < amount; ++i) {
            CrewBlueprint member;
            member.race = race;
            member.name = race + "_" + std::to_string(i + 1);
            member.room = -1;
            crew.push_back(std::move(member));
        }
    }
    for (const auto& c : node.children) collectCrew(c, crew);
}

}

bool parseShipBlueprint(const bxml::Node& node, ShipBlueprint& out) {
    if (node.name != "shipBlueprint" && node.name != "ship") return false;

    out = {};
    out.id = attribute(node, "name");
    if (out.id.empty()) out.id = attribute(node, "id");
    out.name = attribute(node, "shipName");
    if (out.name.empty()) out.name = attribute(node, "name");
    out.layout = attribute(node, "layout");

    if (const auto* layout = child(node, "layout")) {
        if (out.layout.empty()) out.layout = attribute(*layout, "name");
    }

    collectRooms(node, out.rooms);
    collectSystems(node, out.systems);
    if (const auto* health = child(node, "health")) out.maxHealth = integer(*health, "amount", 0);
    if (const auto* power = child(node, "maxPower")) out.startingReactorPower = integer(*power, "amount", 0);
    collectCrew(node, out.crew);
    return true;
}

}

bool wormhole::parseLayoutBlueprint(const std::string& text, wormhole::LayoutBlueprint& out) {
    out = {};
    std::istringstream in(text);
    std::string line;
    auto nextInt = [&](const char* what) {
        std::string value;
        if (!std::getline(in, value)) throw std::runtime_error(std::string("layout missing ") + what);
        return std::stoi(value);
    };
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        if (line == "X_OFFSET") out.xOffset = nextInt("X_OFFSET");
        else if (line == "Y_OFFSET") out.yOffset = nextInt("Y_OFFSET");
        else if (line == "HORIZONTAL") out.horizontal = nextInt("HORIZONTAL");
        else if (line == "VERTICAL") out.vertical = nextInt("VERTICAL");
        else if (line == "ELLIPSE") {
            out.ellipseW = nextInt("ELLIPSE width"); out.ellipseH = nextInt("ELLIPSE height");
            out.ellipseX = nextInt("ELLIPSE x"); out.ellipseY = nextInt("ELLIPSE y");
        } else if (line == "ROOM") {
            wormhole::RoomBlueprint room;
            room.id = nextInt("ROOM id"); room.x = nextInt("ROOM x"); room.y = nextInt("ROOM y");
            room.w = nextInt("ROOM width"); room.h = nextInt("ROOM height");
            if (room.id != static_cast<int>(out.rooms.size()))
                throw std::runtime_error("layout ROOM ids are not contiguous");
            out.rooms.push_back(room);
        } else if (line == "DOOR") {
            wormhole::DoorBlueprint door;
            door.x = nextInt("DOOR x"); door.y = nextInt("DOOR y");
            door.leftRoom = nextInt("DOOR left room"); door.rightRoom = nextInt("DOOR right room");
            door.vertical = nextInt("DOOR orientation") != 0;
            out.doors.push_back(door);
        } else {
            throw std::runtime_error("unknown layout line: " + line);
        }
    }
    if (out.ellipseW <= 0 || out.ellipseH <= 0)
        throw std::runtime_error("layout missing ELLIPSE");
    return true;
}
