#include "platform/input.hpp"
#include <cassert>

int main() {
    wormhole::Input input;
    assert(!input.down(wormhole::Button::Cross));
    assert(!input.pressed(wormhole::Button::Cross));

    input.setButton(wormhole::Button::Cross, true);
    assert(input.down(wormhole::Button::Cross));
    assert(input.pressed(wormhole::Button::Cross));

    input.beginFrame();
    assert(input.down(wormhole::Button::Cross));
    assert(!input.pressed(wormhole::Button::Cross));

    input.setButton(wormhole::Button::Cross, false);
    assert(!input.down(wormhole::Button::Cross));
    assert(!input.pressed(wormhole::Button::Cross));

    return 0;
}
