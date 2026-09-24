#pragma once
#include "render/graphics.hpp"
#include <string>
#include <string_view>

namespace wormhole {

class TextRenderer {
public:
    TextRenderer() = default;
    ~TextRenderer();

    bool init();
    void shutdown(Graphics& graphics);
    bool ready() const { return ready_; }

    void draw(Graphics& graphics, std::string_view text, float x, float y,
              float pixelSize, const Color& color = {});

private:
    struct Impl;
    Impl* impl_{nullptr};
    bool ready_{false};
};

} // namespace wormhole
