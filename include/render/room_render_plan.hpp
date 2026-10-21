#pragma once

#include "render/graphics.hpp"

namespace wormhole {

// Describes the layers used to render one room. The opaque base must be drawn
// before a potentially transparent room texture; status overlays are drawn
// after it so selection and damage remain visible over the artwork.
struct RoomRenderPlan {
    Color baseColor;
    Color selectionOverlayColor;
    Color damageOverlayColor;
    bool drawTexture;
    bool drawSelectionOverlay;
    bool drawDamageOverlay;
};

inline RoomRenderPlan makeRoomRenderPlan(bool textureAvailable, bool selected, int damage) {
    return {
        {0.10f, 0.18f, 0.25f, 1.0f},
        {0.25f, 0.38f, 0.48f, 0.35f},
        {0.45f, 0.08f, 0.05f, 0.30f},
        textureAvailable,
        selected,
        !selected && damage > 0
    };
}

} // namespace wormhole
