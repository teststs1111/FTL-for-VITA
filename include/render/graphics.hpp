#pragma once
#include "render/texture.hpp"
#include <cstdint>
#include <vector>
namespace wormhole {
struct Color { float r{1.f},g{1.f},b{1.f},a{1.f}; };
class Graphics {
public:
 bool init(); void shutdown(); void beginFrame(const Color& clear); void endFrame();
 void fillRect(float x,float y,float w,float h,const Color& color);
 void drawLine(float x1,float y1,float x2,float y2,const Color& color);
 Texture createTexture(const std::vector<std::uint8_t>& rgba,int width,int height);
 void destroyTexture(Texture& texture);
 void drawTexture(const Texture& texture,float x,float y,float w,float h,const Color& color={});
 void drawTextureRegion(const Texture& texture,float x,float y,float w,float h,float u0,float v0,float u1,float v1,const Color& color={});
private: bool initialized_{false};
};
}
