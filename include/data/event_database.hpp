#pragma once
#include "data/bxml.hpp"
#include "data/asset_store.hpp"
#include <string>
#include <cstdint>
#include <vector>
#include <unordered_map>

namespace wormhole {

struct EventDamageEffect {
    int amount{0};
    std::string system;
    std::string effect;
};

struct EventCrewMemberEffect {
    int amount{0};
    std::string id;
    std::string race;
    bool allSkills{false};
    int pilot{0};
    int engines{0};
    int shields{0};
    int weapons{0};
    int repair{0};
    int combat{0};
};

struct EventCrewRemovalEffect {
    bool clone{false};
    std::string race;
    std::string textKey;
};

struct EventBoarderEffect {
    int min{1};
    int max{1};
    std::string race{"human"};
    int maxGroup{0};
};

struct EventChoice {
    std::string text;
    std::string textKey;
    std::string load;
    std::string requirement;
    int requirementLevel{0};
    bool blue{false};
    bool hostile{false};
    std::string hostileShipId;
    std::string questId;
    std::string questTargetId;
    bool store{false};
    bool repair{false};
    int scrap{0};
    int scrapMax{0};
    int fuel{0};
    int fuelMax{0};
    int missiles{0};
    int missilesMax{0};
    int drones{0};
    int dronesMax{0};
    std::vector<EventDamageEffect> effects;
    std::vector<EventCrewMemberEffect> crewMembers;
    std::vector<EventCrewRemovalEffect> crewRemovals;
    std::vector<EventBoarderEffect> boarders;
};

struct EventDefinition {
    std::string id;
    std::string text;
    std::string textKey;
    std::vector<EventChoice> choices;
    bool hostile{false};
    std::string hostileShipId;
    std::string questId;
    std::string questTargetId;
    bool store{false};
    bool repair{false};
    int initialScrap{0};
    int initialScrapMax{0};
    int initialFuel{0};
    int initialFuelMax{0};
    int initialMissiles{0};
    int initialMissilesMax{0};
    int initialDrones{0};
    int initialDronesMax{0};
    std::vector<EventDamageEffect> effects;
    std::vector<EventCrewMemberEffect> crewMembers;
    std::vector<EventCrewRemovalEffect> crewRemovals;
    std::vector<EventBoarderEffect> boarders;
    bool valid{false};
};

class EventDatabase {
public:
    explicit EventDatabase(AssetStore& assets) : assets_(assets) {}
    bool load();
    void setAdvancedEdition(bool enabled) { advancedEdition_ = enabled; }
    const EventDefinition* find(const std::string& id) const;
    const EventDefinition* resolve(const std::string& id, std::uint32_t seed) const;
    enum class BeaconType { Empty, Hostile, Store, Distress, Quest, Exit };
    BeaconType classify(const std::string& id) const;
    const EventDefinition* firstUsable() const;
    const std::vector<std::string>& ids() const { return order_; }
    std::size_t size() const { return events_.size(); }

private:
    struct EventPoolEntry { std::string id; int weight{1}; };
    struct EventPool { std::vector<EventPoolEntry> entries; };

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
    bool advancedEdition_{true};
    bool replacingPools_{false};
    bool replacingEvents_{false};
};

}
