#include "render/texture_cache.hpp"
#include "render/png_loader.hpp"
namespace wormhole {
bool TextureCache::load(Graphics& graphics,AssetStore& assets,const std::string& name) {
 if(textures_.find(name)!=textures_.end()) return true;
 const auto* bytes=assets.getBytes(name); if(!bytes) return false;
 RgbaImage image; if(!decodePng(*bytes,image)) return false;
 Texture texture=graphics.createTexture(image.pixels,image.width,image.height);
#ifdef __vita__
 if(!texture.valid()) return false;
#else
 if(image.width<=0||image.height<=0) return false;
#endif
 textures_.emplace(name,texture); return true;
}
const Texture* TextureCache::get(const std::string& name) const {
 auto it=textures_.find(name); return it==textures_.end()?nullptr:&it->second;
}
void TextureCache::clear(Graphics& graphics) {
 for(auto& item:textures_) graphics.destroyTexture(item.second);
 textures_.clear();
}
}
