#include "data/event_database.hpp"

#include <algorithm>
#include <cctype>

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

void EventDatabase::collectEvents(const bxml::Node& node) {
    if (node.name == "event") {
        const auto idIt = node.attributes.find("name");
        if (idIt != node.attributes.end() && !idIt->second.empty()) {
            EventDefinition event;
            event.id = idIt->second;
            event.text = nodeText(node);
            event.hostile = hasChild(node, "ship") &&
                child(node, "ship")->attributes.count("hostile") &&
                child(node, "ship")->attributes.at("hostile") == "true";
            event.store = hasChild(node, "store");
            event.repair = hasChild(node, "repair");

            for (const auto& c : node.children) {
                if (c.name != "choice") continue;
                if (c.attributes.count("hidden") && c.attributes.at("hidden") == "true")
                    continue;
                EventChoice choice;
                if (const auto* t = child(c, "text")) choice.text = nodeText(*t);
                if (const auto* e = child(c, "event")) {
                    const auto it = e->attributes.find("load");
                    if (it != e->attributes.end()) choice.load = it->second;
                    choice.hostile = hasChild(*e, "ship") &&
                        child(*e, "ship")->attributes.count("hostile") &&
                        child(*e, "ship")->attributes.at("hostile") == "true";
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
    }

    for (const auto& c : node.children) collectEvents(c);
}

bool EventDatabase::load() {
    events_.clear();
    order_.clear();

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
