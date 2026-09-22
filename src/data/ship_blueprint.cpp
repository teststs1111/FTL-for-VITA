#include "data/ship_blueprint.hpp"
#include <cstdlib>

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
    if (node.name == "system" || node.name == "systemList") {
        if (node.name == "system") {
            SystemSlotBlueprint system;
            system.system = attribute(node, "type");
            if (system.system.empty()) system.system = attribute(node, "name");
            system.room = integer(node, "room", -1);
            system.level = integer(node, "level");
            systems.push_back(std::move(system));
        }
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
    collectCrew(node, out.crew);
    return true;
}

}
