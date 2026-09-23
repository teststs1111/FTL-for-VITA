#include "data/blueprint_database.hpp"

#include <functional>

namespace wormhole {

bool BlueprintDatabase::loadShipBlueprint(const std::string& assetPath) {
    const auto* data = assets_.getBytes(assetPath);
    if (!data) return false;

    bxml::Node root;
    try {
        root = bxml::read(*data);
    } catch (...) {
        return false;
    }

    ShipBlueprint ship;
    if (!parseShipBlueprint(root, ship) || ship.id.empty()) return false;
    ships_[ship.id] = std::move(ship);
    return true;
}

std::size_t BlueprintDatabase::loadShipBlueprints(const std::string& assetPath) {
    const auto* data = assets_.getBytes(assetPath);
    if (!data) return 0;

    bxml::Node root;
    try {
        root = bxml::read(*data);
    } catch (...) {
        return 0;
    }

    std::size_t loaded = 0;
    std::function<void(const bxml::Node&)> visit = [&](const bxml::Node& node) {
        if (node.name == "droneBlueprint") {
            DroneBlueprint drone;
            const auto getText = [&](const char* name) -> std::string {
                for (const auto& child : node.children)
                    if (child.name == name) return child.text;
                return {};
            };
            const auto toInt = [&](const std::string& value, int fallback) {
                if (value.empty()) return fallback;
                try { return std::stoi(value); } catch (...) { return fallback; }
            };
            const auto attr = [&](const char* name) -> std::string {
                const auto it = node.attributes.find(name);
                return it == node.attributes.end() ? std::string{} : it->second;
            };
            const auto value = [&](const char* name) -> std::string {
                const auto text = getText(name);
                return text.empty() ? attr(name) : text;
            };
            drone.name = attr("name");
            const std::string type = value("type");
            if (type == "COMBAT") drone.type = DroneBlueprint::Type::Combat;
            else if (type == "SHIP_REPAIR") drone.type = DroneBlueprint::Type::ShipRepair;
            else if (type == "DEFENSE") drone.type = DroneBlueprint::Type::Defense;
            else if (type == "REPAIR") drone.type = DroneBlueprint::Type::Repair;
            else if (type == "BATTLE") drone.type = DroneBlueprint::Type::Battle;
            else if (type == "BOARDER") drone.type = DroneBlueprint::Type::Boarding;
            else if (type == "HACKING") drone.type = DroneBlueprint::Type::Hacking;
            else if (type == "SHIELD") drone.type = DroneBlueprint::Type::Shield;
            drone.power = toInt(value("power"), 1);
            drone.speed = toInt(value("speed"), 0);
            drone.droneImage = value("droneImage");
            drone.iconImage = value("iconImage");
            drone.cooldown = toInt(value("cooldown"), 0);
            drone.defenceTarget = value("target");
            drone.dodge = toInt(value("dodge"), 0);
            drone.weaponBlueprint = value("weaponBlueprint");
            drone.cost = toInt(value("cost"), 0);
            if (!drone.name.empty()) drones_[drone.name] = std::move(drone);
        } else f (node.name == "weaponBlueprint") {
            WeaponBlueprint weapon;
            const auto getText = [&](const char* name) -> std::string {
                for (const auto& child : node.children)
                    if (child.name == name) return child.text;
                return {};
            };
            const auto toInt = [&](const std::string& value, int fallback) {
                if (value.empty()) return fallback;
                try { return std::stoi(value); } catch (...) { return fallback; }
            };
            const auto toFloat = [&](const std::string& value, float fallback) {
                if (value.empty()) return fallback;
                try { return std::stof(value); } catch (...) { return fallback; }
            };
            const auto attr = [&](const char* name) -> std::string {
                const auto it = node.attributes.find(name);
                return it == node.attributes.end() ? std::string{} : it->second;
            };
            const auto value = [&](const char* name) -> std::string {
                const auto text = getText(name);
                if (!text.empty()) return text;
                return attr(name);
            };
            weapon.name = attr("name");
            weapon.type = value("type");
            weapon.launcher = attr("weaponArt");
            if (weapon.launcher.empty()) weapon.launcher = value("weaponArt");
            weapon.projectile = value("image");
            weapon.shots = toInt(value("shots"), 1);
            weapon.damage = toInt(value("damage"), 0);
            weapon.systemDamage = toInt(value("sysDamage"), 0);
            weapon.ionDamage = toInt(value("ionDamage"), toInt(value("ion"), 0));
            weapon.shieldPiercing = toInt(value("sp"), 0);
            weapon.missilesUsed = toInt(value("missiles"), 0);
            weapon.speed = toInt(value("speed"), 0);
            weapon.personnelDamage = toInt(value("personnelDamage"), toInt(value("persDamage"), 0));
            weapon.hullBust = toInt(value("hullBust"), 0);
            weapon.fireChance = toInt(value("fireChance"), 0);
            weapon.breachChance = toInt(value("breachChance"), 0);
            weapon.stunChance = toInt(value("stunChance"), 0);
            weapon.stunDuration = toInt(value("stun"), 0);
            weapon.power = toInt(value("power"), 1);
            weapon.cooldown = toFloat(value("cooldown"), 5.0f);
            weapon.cost = toInt(value("cost"), 0);
            if (!weapon.name.empty()) weapons_[weapon.name] = std::move(weapon);
        } else if (node.name == "shipBlueprint" || node.name == "ship") {
            ShipBlueprint ship;
            if (parseShipBlueprint(node, ship) && !ship.id.empty()) {
                ships_[ship.id] = std::move(ship);
                ++loaded;
            }
        }
        for (const auto& child : node.children) visit(child);
    };
    visit(root);
    return loaded;
}

const ShipBlueprint* BlueprintDatabase::findShip(const std::string& id) const {
    const auto it = ships_.find(id);
    return it == ships_.end() ? nullptr : &it->second;
}

const WeaponBlueprint* BlueprintDatabase::findWeapon(const std::string& id) const {
    const auto it = weapons_.find(id);
    return it == weapons_.end() ? nullptr : &it->second;
}

const DroneBlueprint* BlueprintDatabase::findDrone(const std::string& id) const {
    const auto it = drones_.find(id);
    return it == drones_.end() ? nullptr : &it->second;
}

void BlueprintDatabase::clear() {
    ships_.clear();
    weapons_.clear();
    drones_.clear();
}

}
