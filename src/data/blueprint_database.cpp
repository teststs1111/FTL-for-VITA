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
        if (node.name == "weaponBlueprint") {
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
            weapon.name = attr("name");
            weapon.type = getText("type");
            weapon.launcher = getText("weaponArt");
            weapon.projectile = getText("image");
            weapon.shots = toInt(getText("shots"), 1);
            weapon.damage = toInt(getText("damage"), 0);
            weapon.systemDamage = toInt(getText("sysDamage"), 0);
            weapon.ionDamage = toInt(getText("ion"), 0);
            weapon.shieldPiercing = toInt(getText("sp"), 0);
            weapon.missilesUsed = toInt(getText("missiles"), 0);
            weapon.speed = toInt(getText("speed"), 0);
            weapon.personnelDamage = toInt(getText("persDamage"), 0);
            weapon.hullBust = toInt(getText("hullBust"), 0);
            weapon.fireChance = toInt(getText("fireChance"), 0);
            weapon.breachChance = toInt(getText("breachChance"), 0);
            weapon.stunChance = toInt(getText("stunChance"), 0);
            weapon.stunDuration = toInt(getText("stun"), 0);
            weapon.power = toInt(getText("power"), 1);
            weapon.cooldown = toFloat(getText("cooldown"), 5.0f);
            weapon.cost = toInt(getText("cost"), 0);
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

void BlueprintDatabase::clear() {
    ships_.clear();
    weapons_.clear();
}

}
