#include "render/texture.hpp"
namespace wormhole {
Texture::Texture(int width, int height, std::uint32_t handle) : width_(width), height_(height), handle_(handle) {}
}
