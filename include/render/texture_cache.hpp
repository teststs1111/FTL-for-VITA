#pragma once
#include "data/asset_store.hpp"
#include "render/graphics.hpp"
#include <string>
#include <unordered_map>
namespace wormhole {
class TextureCache {
public:
 bool load(Graphics& graphics,AssetStore& assets,const std::string& name);
 const Texture* get(const std::string& name) const;
 void clear(Graphics& graphics);
private:
 std::unordered_map<std::string,Texture> textures_;
};
}
