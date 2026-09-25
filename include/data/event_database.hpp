#pragma once
#include "data/bxml.hpp"
#include "data/asset_store.hpp"
#include <string>
#include <vector>
#include <unordered_map>

namespace wormhole {

struct EventChoice {
    std::string text;
    std::string load;
    bool hostile{false};
    bool store{false};
    bool repair{false};
    int scrap{0};
    int fuel{0};
    int missiles{0};
    int drones{0};
};

struct EventDefinition {
    std::string id;
    std::string text;
    std::vector<EventChoice> choices;
    bool hostile{false};
    bool store{false};
    bool repair{false};
    bool valid{false};
};

class EventDatabase {
public:
    explicit EventDatabase(AssetStore& assets) : assets_(assets) {}
    bool load();
    const EventDefinition* find(const std::string& id) const;
    const EventDefinition* firstUsable() const;
    const std::vector<std::string>& ids() const { return order_; }
    std::size_t size() const { return events_.size(); }

private:
    void collectEvents(const bxml::Node& node);
    void addEvent(const bxml::Node& node, const std::string& id);
    static std::string nodeText(const bxml::Node& node);
    static bool hasChild(const bxml::Node& node, const std::string& name);
    static const bxml::Node* child(const bxml::Node& node, const std::string& name);
    static int attrInt(const bxml::Node& node, const char* name, int fallback = 0);

    AssetStore& assets_;
    std::unordered_map<std::string, EventDefinition> events_;
    std::unordered_map<std::string, EventPool> eventPools_;
    std::vector<std::string> order_;
};

}
