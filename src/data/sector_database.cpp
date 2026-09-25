#include "data/sector_database.hpp"
#include <algorithm>
#include <cctype>

namespace wormhole {
namespace {
bool sectorFile(const std::string& n) {
    return n == "data/sector_data.xml" || n == "data/sector_data_ae.xml";
}
int attrInt(const bxml::Node& n, const char* key, int fallback) {
    const auto it=n.attributes.find(key);
    if(it==n.attributes.end()) return fallback;
    try{return std::stoi(it->second);}catch(...){return fallback;}
}
bool attrBool(const bxml::Node& n,const char* key) {
    const auto it=n.attributes.find(key);
    return it!=n.attributes.end() && (it->second=="true" || it->second=="1");
}
}
void SectorDatabase::collect(const bxml::Node& node) {
    if(node.name=="sectorDescription") {
        const auto it=node.attributes.find("name");
        if(it!=node.attributes.end()) {
            SectorDefinition s;
            s.name=it->second;
            s.minSector=attrInt(node,"minSector",0);
            s.unique=attrBool(node,"unique");
            for(const auto& c:node.children) {
                if(c.name=="startEvent") s.startEvent=c.text;
                else if(c.name=="event") {
                    const auto e=c.attributes.find("name");
                    if(e!=c.attributes.end()) {
                        SectorEventPool p;
                        p.name=e->second;
                        p.min=attrInt(c,"min",0);
                        p.max=attrInt(c,"max",p.min);
                        s.events.push_back(std::move(p));
                    }
                }
            }
            sectors_.push_back(std::move(s));
        }
    }
    for(const auto& c:node.children) collect(c);
}
bool SectorDatabase::load() {
    sectors_.clear();
    for(const auto& n:assets_.fileNames()) {
        if(!sectorFile(n)) continue;
        const auto* bytes=assets_.getBytes(n);
        if(!bytes || bytes->empty()) continue;
        try { collect(bxml::read(*bytes)); } catch(...) {}
    }
    return !sectors_.empty();
}
const SectorDefinition* SectorDatabase::select(int sector,std::size_t variant) const {
    if(sectors_.empty()) return nullptr;
    const SectorDefinition* finalSector=nullptr;
    for(const auto& s:sectors_) {
        if(s.minSector>sector) continue;
        if(s.name=="FINAL") finalSector=&s;
    }
    if(sector>=7 && finalSector) return finalSector;
    std::vector<const SectorDefinition*> candidates;
    for(const auto& s:sectors_) {
        if(s.name=="FINAL" || s.events.empty() || s.minSector>sector) continue;
        candidates.push_back(&s);
    }
    if(candidates.empty()) return nullptr;
    return candidates[variant%candidates.size()];
}
}
