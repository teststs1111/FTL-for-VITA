#pragma once
#include <cstdint>
namespace wormhole {
class Texture {
public:
 Texture()=default; Texture(int width,int height,std::uint32_t handle);
 int width() const{return width_;} int height() const{return height_;}
 std::uint32_t handle() const{return handle_;} bool valid() const{return handle_!=0;}
private: int width_{0}; int height_{0}; std::uint32_t handle_{0};
};
}
