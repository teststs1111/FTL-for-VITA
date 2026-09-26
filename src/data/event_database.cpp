#include "data/event_database.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>

namespace wormhole {

namespace {
bool endsWith(const std::string& s, const char* suffix) {
    const std::string t(suffix);
    return s.size() >= t.size() && s.compare(s.size() - t.size(), t.size(), t) == 0;
}

bool startsWithEvents(const std::string& s) {
    return s.rfind("data/events", 0) == 0 && endsWith(s, ".xml");
}

const bxml::Node* findFirst(const bxml::Node& node, const std::string& name) {
    if (node.name == name) return &node;
    for (const auto& c : node.children) {
        if (const auto* found = findFirst(c, name)) return found;
    }
    return nullptr;
}
}

static int crewAttrInt(const bxml::Node& node, const char* name, int fallback = 0) {
    const auto it = node.attributes.find(name);
    if (it == node.attributes.end()) return fallback;
    try { return std::stoi(it->second); } catch (...) { return fallback; }
}

static const bxml::Node* crewChild(const bxml::Node& node, const std::string& name) {
    for (const auto& c : node.children) if (c.name == name) return &c;
    return nullptr;
}

static std::string crewNodeText(const bxml::Node& node) {
    if (!node.text.empty()) return node.text;
    for (const auto& c : node.children) if (c.name == "text" && !c.text.empty()) return c.text;
    return {};
}

static void parseCrewEffects(const bxml::Node& node,
                              std::vector<EventCrewMemberEffect>& members,
                              std::vector<EventCrewRemovalEffect>& removals,
                              std::vector<EventBoarderEffect>& boarders) {
    for (const auto& c : node.children) {
        if (c.name == "crewMember") {
            EventCrewMemberEffect effect;
            effect.amount = crewAttrInt(c, "amount", 0);
            auto it = c.attributes.find("id");
            if (it != c.attributes.end()) effect.id = it->second;
            it = c.attributes.find("class");
            if (it != c.attributes.end()) effect.race = it->second;
            it = c.attributes.find("all_skills");
            effect.allSkills = it != c.attributes.end() && it->second == "1";
            effect.pilot = crewAttrInt(c, "pilot", 0);
            effect.engines = crewAttrInt(c, "engines", 0);
            effect.shields = crewAttrInt(c, "shields", 0);
            effect.weapons = crewAttrInt(c, "weapons", 0);
            effect.repair = crewAttrInt(c, "repair", 0);
            effect.combat = crewAttrInt(c, "combat", 0);
            if (effect.amount != 0) members.push_back(std::move(effect));
        } else if (c.name == "removeCrew") {
            EventCrewRemovalEffect effect;
            auto it = c.attributes.find("class");
            if (it != c.attributes.end()) effect.race = it->second;
            if (const auto* clone = crewChild(c, "clone")) {
                effect.clone = crewNodeText(*clone) == "true";
            }
            if (const auto* text = crewChild(c, "text")) {
                const auto idIt = text->attributes.find("id");
                if (idIt != text->attributes.end()) effect.textKey = idIt->second;
            }
            removals.push_back(std::move(effect));
        } else if (c.name == "boarders") {
            EventBoarderEffect effect;
            effect.min = std::max(1, crewAttrInt(c, "min", 1));
            effect.max = std::max(effect.min, crewAttrInt(c, "max", effect.min));
            auto it = c.attributes.find("class");
            if (it != c.attributes.end() && !it->second.empty()) effect.race = it->second;
            effect.maxGroup = std::max(0, crewAttrInt(c, "max_group", 0));
            boarders.push_back(std::move(effect));
        }
    }
}



std::string EventDatabase::nodeText(const bxml::Node& node) {
    std::string out = node.text;
    for (const auto& child : node.children) {
        if (child.name == "text" && !child.text.empty()) {
            out = child.text;
            break;
        }
    }
    return out;
}

bool EventDatabase::hasChild(const bxml::Node& node, const std::string& name) {
    return std::any_of(node.children.begin(), node.children.end(),
        [&](const bxml::Node& c) { return c.name == name; });
}

const bxml::Node* EventDatabase::child(const bxml::Node& node, const std::string& name) {
    for (const auto& c : node.children)
        if (c.name == name) return &c;
    return nullptr;
}

int EventDatabase::attrInt(const bxml::Node& node, const char* name, int fallback) {
    const auto it = node.attributes.find(name);
    if (it == node.attributes.end()) return fallback;
    try { return std::stoi(it->second); } catch (...) { return fallback; }
}

void EventDatabase::addEvent(const bxml::Node& node, const std::string& id) {
    if (id.empty() || node.name != "event") return;
    EventDefinition event;
    event.id = id;
    event.text = nodeText(node);
    if (const auto* textNode = child(node, "text")) {
        const auto idIt = textNode->attributes.find("id");
        if (idIt != textNode->attributes.end()) event.textKey = idIt->second;
    }
    if (const auto* ship = child(node, "ship")) {
        const auto it = ship->attributes.find("hostile");
        event.hostile = it != ship->attributes.end() && it->second == "true";
        const auto name = ship->attributes.find("name");
        if (name != ship->attributes.end()) event.hostileShipId = name->second;
    }
    if (const auto* quest = child(node, "quest")) {
        const auto target = quest->attributes.find("event");
        if (target != quest->attributes.end()) event.questTargetId = target->second;
        const auto name = quest->attributes.find("name");
        if (name != quest->attributes.end()) event.questId = name->second;
        else {
            const auto idIt = quest->attributes.find("id");
            if (idIt != quest->attributes.end()) event.questId = idIt->second;
        }
    }
    event.store = hasChild(node, "store");
    event.repair = hasChild(node, "repair");
    event.distressBeacon = hasChild(node, "distressBeacon");
    if (const auto* env = child(node, "environment")) {
        event.hasEnvironment = true;
        auto it = env->attributes.find("type");
        if (it != env->attributes.end()) event.environment.type = it->second;
        it = env->attributes.find("target");
        if (it != env->attributes.end()) event.environment.target = it->second;
    }
    if (const auto* weapon = child(node, "weapon")) {
        auto it = weapon->attributes.find("name");
        if (it != weapon->attributes.end()) event.weaponReward = it->second;
    }
    if (const auto* reward = child(node, "autoReward")) {
        event.hasAutoReward = true;
        auto it = reward->attributes.find("level");
        if (it != reward->attributes.end()) event.autoReward.level = it->second;
        event.autoReward.type = nodeText(*reward);
    }
    parseCrewEffects(node, event.crewMembers, event.crewRemovals, event.boarders);

    // Preserve item_modify directly attached to an event. These effects are
    // applied when the event is entered, rather than only after a choice.
    if (const auto* items = child(node, "item_modify")) {
        for (const auto& item : items->children) {
            if (item.name != "item") continue;
            const auto type = item.attributes.find("type");
            if (type == item.attributes.end()) continue;
            const int minAmount = attrInt(item, "min", attrInt(item, "amount", 0));
            const int maxAmount = attrInt(item, "max", minAmount);
            if (type->second == "scrap") { event.initialScrap += minAmount; event.initialScrapMax += std::max(minAmount, maxAmount); }
            else if (type->second == "fuel") { event.initialFuel += minAmount; event.initialFuelMax += std::max(minAmount, maxAmount); }
            else if (type->second == "missiles") { event.initialMissiles += minAmount; event.initialMissilesMax += std::max(minAmount, maxAmount); }
            else if (type->second == "drones") { event.initialDrones += minAmount; event.initialDronesMax += std::max(minAmount, maxAmount); }
        }
    }

    for (const auto& c : node.children) {
        if (c.name != "choice") continue;
        if (c.attributes.count("hidden") && c.attributes.at("hidden") == "true") continue;
        EventChoice choice;
        const auto reqIt = c.attributes.find("req");
        if (reqIt != c.attributes.end()) choice.requirement = reqIt->second;
        choice.requirementLevel = attrInt(c, "lvl", 0);
        const auto blueIt = c.attributes.find("blue");
        choice.blue = blueIt != c.attributes.end() && blueIt->second == "true";
        if (const auto* t = child(c, "text")) {
            choice.text = nodeText(*t);
            const auto idIt = t->attributes.find("id");
            if (idIt != t->attributes.end()) choice.textKey = idIt->second;
        }
        if (const auto* e = child(c, "event")) {
            const auto it = e->attributes.find("load");
            if (it != e->attributes.end()) choice.load = it->second;
            if (const auto* ship = child(*e, "ship")) {
                const auto hit = ship->attributes.find("hostile");
                choice.hostile = hit != ship->attributes.end() && hit->second == "true";
                const auto name = ship->attributes.find("name");
                if (name != ship->attributes.end()) choice.hostileShipId = name->second;
            }
            if (const auto* quest = child(*e, "quest")) {
                const auto target = quest->attributes.find("event");
                if (target != quest->attributes.end()) choice.questTargetId = target->second;
                const auto name = quest->attributes.find("name");
                if (name != quest->attributes.end()) choice.questId = name->second;
                else {
                    const auto idIt = quest->attributes.find("id");
                    if (idIt != quest->attributes.end()) choice.questId = idIt->second;
                }
            }
            choice.store = hasChild(*e, "store");
            choice.repair = hasChild(*e, "repair");
            choice.distressBeacon = hasChild(*e, "distressBeacon");
            if (const auto* env = child(*e, "environment")) {
                choice.hasEnvironment = true;
                auto it = env->attributes.find("type");
                if (it != env->attributes.end()) choice.environment.type = it->second;
                it = env->attributes.find("target");
                if (it != env->attributes.end()) choice.environment.target = it->second;
            }
            if (const auto* weapon = child(*e, "weapon")) {
                auto it = weapon->attributes.find("name");
                if (it != weapon->attributes.end()) choice.weaponReward = it->second;
            }
            if (const auto* reward = child(*e, "autoReward")) {
                choice.hasAutoReward = true;
                auto it = reward->attributes.find("level");
                if (it != reward->attributes.end()) choice.autoReward.level = it->second;
                choice.autoReward.type = nodeText(*reward);
            }
            parseCrewEffects(*e, choice.crewMembers, choice.crewRemovals, choice.boarders);
            if (const auto* items = child(*e, "item_modify")) {
                for (const auto& item : items->children) {
                    if (item.name != "item") continue;
                    const auto type = item.attributes.find("type");
                    if (type == item.attributes.end()) continue;
                    const int minAmount = attrInt(item, "min", attrInt(item, "amount", 0));
                    const int maxAmount = attrInt(item, "max", minAmount);
                    if (type->second == "scrap") { choice.scrap += minAmount; choice.scrapMax += std::max(minAmount, maxAmount); }
                    else if (type->second == "fuel") { choice.fuel += minAmount; choice.fuelMax += std::max(minAmount, maxAmount); }
                    else if (type->second == "missiles") { choice.missiles += minAmount; choice.missilesMax += std::max(minAmount, maxAmount); }
                    else if (type->second == "drones") { choice.drones += minAmount; choice.dronesMax += std::max(minAmount, maxAmount); }
                }
            }
        }
        if (!choice.text.empty() || !choice.load.empty() || choice.store || choice.hostile)
            event.choices.push_back(std::move(choice));
    }

    event.valid = !event.text.empty() || !event.choices.empty() ||
                  event.hostile || event.store || event.repair;
    if (!event.valid) return;
    const auto it = events_.find(event.id);
    if (it == events_.end()) {
        order_.push_back(event.id);
        events_.emplace(event.id, std::move(event));
    } else if (replacingEvents_) {
        it->second = std::move(event);
    }
}

void EventDatabase::collectEvents(const bxml::Node& node) {
    if (node.name == "event") {
        const auto idIt = node.attributes.find("name");
        if (idIt != node.attributes.end()) addEvent(node, idIt->second);
    } else if (node.name == "eventList") {
        const auto idIt = node.attributes.find("name");
        if (idIt != node.attributes.end()) {
            EventPool pool;
            for (const auto& c : node.children) {
                if (c.name != "event") continue;
                std::string eventId;
                const auto loadIt = c.attributes.find("load");
                if (loadIt != c.attributes.end()) eventId = loadIt->second;
                const auto nameIt = c.attributes.find("name");
                if (eventId.empty() && nameIt != c.attributes.end()) eventId = nameIt->second;
                if (eventId.empty()) continue;
                pool.entries.push_back({eventId, std::max(1, attrInt(c, "weight", 1))});
                if (nameIt != c.attributes.end() && !nameIt->second.empty())
                    addEvent(c, nameIt->second);
            }
            if (!pool.entries.empty()) {
                std::string poolId = idIt->second;
                const std::string prefix = "OVERRIDE_";
                if (poolId.rfind(prefix, 0) == 0) poolId.erase(0, prefix.size());
                if (replacingPools_ || poolId != idIt->second)
                    eventPools_[poolId] = std::move(pool);
                else if (eventPools_.find(poolId) == eventPools_.end())
                    eventPools_.emplace(poolId, std::move(pool));
            }
        }
    }
    for (const auto& c : node.children) collectEvents(c);
}

const EventDefinition* EventDatabase::resolve(const std::string& id, std::uint32_t seed) const {
    if (const auto* direct = find(id)) return direct;
    const auto it = eventPools_.find(id);
    if (it == eventPools_.end() || it->second.entries.empty()) return nullptr;
    std::uint32_t total = 0;
    for (const auto& entry : it->second.entries)
        total += static_cast<std::uint32_t>(std::max(1, entry.weight));
    if (total == 0) return nullptr;
    std::uint32_t pick = seed % total;
    for (const auto& entry : it->second.entries) {
        const auto weight = static_cast<std::uint32_t>(std::max(1, entry.weight));
        if (pick < weight) return find(entry.id);
        pick -= weight;
    }
    return find(it->second.entries.front().id);
}

bool EventDatabase::load() {
    events_.clear();
    order_.clear();
    eventPools_.clear();

    replacingPools_ = false;
    replacingEvents_ = false;
    for (const auto& name : assets_.fileNames()) {
        if (!startsWithEvents(name)) continue;
        if (!advancedEdition_ && (name == "data/events_ae.xml" || name == "data/events_ae_overwrite.xml")) continue;
        const auto* bytes = assets_.getBytes(name);
        if (!bytes || bytes->empty()) continue;
        try { collectEvents(bxml::read(*bytes)); } catch (...) {}
    }
    if (advancedEdition_) {
        const char* extra[] = {"data/dlcEvents.xml"};
        for (const char* name : extra) {
            const auto* bytes = assets_.getBytes(name);
            if (!bytes || bytes->empty()) continue;
            try { collectEvents(bxml::read(*bytes)); } catch (...) {}
        }
        const auto* overwrite = assets_.getBytes("data/dlcEventsOverwrite.xml");
        if (overwrite && !overwrite->empty()) {
            replacingPools_ = true;
            replacingEvents_ = true;
            try { collectEvents(bxml::read(*overwrite)); } catch (...) {}
            replacingEvents_ = false;
            replacingPools_ = false;
        }
    }
    return !events_.empty();
}

EventDatabase::BeaconType EventDatabase::classify(const std::string& id) const {
    const auto* event = find(id);
    if (!event) return BeaconType::Empty;
    if (event->store) return BeaconType::Store;
    std::string lower = id;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (lower.find("exit") != std::string::npos ||
        lower.find("base") != std::string::npos ||
        lower.find("final") != std::string::npos)
        return BeaconType::Exit;
    if (lower.find("quest") != std::string::npos ||
        lower.find("mission") != std::string::npos)
        return BeaconType::Quest;
    if (lower.find("distress") != std::string::npos ||
        lower.find("rescue") != std::string::npos)
        return BeaconType::Distress;
    if (event->hostile) return BeaconType::Hostile;
    return BeaconType::Empty;
}

const EventDefinition* EventDatabase::find(const std::string& id) const {
    const auto it = events_.find(id);
    return it == events_.end() ? nullptr : &it->second;
}

const EventDefinition* EventDatabase::firstUsable() const {
    for (const auto& id : order_) {
        const auto* event = find(id);
        if (event && event->valid) return event;
    }
    return nullptr;
}

}
