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
    if (const auto* ship = child(node, "ship")) {
        const auto it = ship->attributes.find("hostile");
        event.hostile = it != ship->attributes.end() && it->second == "true";
    }
    event.store = hasChild(node, "store");
    event.repair = hasChild(node, "repair");

    for (const auto& c : node.children) {
        if (c.name != "choice") continue;
        if (c.attributes.count("hidden") && c.attributes.at("hidden") == "true") continue;
        EventChoice choice;
        const auto reqIt = c.attributes.find("req");
        if (reqIt != c.attributes.end()) choice.requirement = reqIt->second;
        choice.requirementLevel = attrInt(c, "lvl", 0);
        const auto blueIt = c.attributes.find("blue");
        choice.blue = blueIt != c.attributes.end() && blueIt->second == "true";
        if (const auto* t = child(c, "text")) choice.text = nodeText(*t);
        if (const auto* e = child(c, "event")) {
            const auto it = e->attributes.find("load");
            if (it != e->attributes.end()) choice.load = it->second;
            if (const auto* ship = child(*e, "ship")) {
                const auto hit = ship->attributes.find("hostile");
                choice.hostile = hit != ship->attributes.end() && hit->second == "true";
            }
            choice.store = hasChild(*e, "store");
            choice.repair = hasChild(*e, "repair");
            if (const auto* items = child(*e, "item_modify")) {
                for (const auto& item : items->children) {
                    if (item.name != "item") continue;
                    const auto type = item.attributes.find("type");
                    if (type == item.attributes.end()) continue;
                    const int amount = attrInt(item, "min", 0);
                    if (type->second == "scrap") choice.scrap += amount;
                    else if (type->second == "fuel") choice.fuel += amount;
                    else if (type->second == "missiles") choice.missiles += amount;
                    else if (type->second == "drones") choice.drones += amount;
                }
            }
        }
        if (!choice.text.empty() || !choice.load.empty() || choice.store || choice.hostile)
            event.choices.push_back(std::move(choice));
    }

    event.valid = !event.text.empty() || !event.choices.empty() ||
                  event.hostile || event.store || event.repair;
    if (event.valid && events_.find(event.id) == events_.end()) {
        order_.push_back(event.id);
        events_.emplace(event.id, std::move(event));
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
                const auto nameIt = c.attributes.find("name");
                if (nameIt == c.attributes.end() || nameIt->second.empty()) continue;
                pool.entries.push_back({nameIt->second, std::max(1, attrInt(c, "weight", 1))});
                addEvent(c, nameIt->second);
            }
            if (!pool.entries.empty() && eventPools_.find(idIt->second) == eventPools_.end())
                eventPools_.emplace(idIt->second, std::move(pool));
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

    for (const auto& name : assets_.fileNames()) {
        if (!startsWithEvents(name)) continue;
        const auto* bytes = assets_.getBytes(name);
        if (!bytes || bytes->empty()) continue;
        try {
            collectEvents(bxml::read(*bytes));
        } catch (...) {
            // One malformed/non-standard optional XML must not prevent the
            // remaining FTL event files from loading.
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
