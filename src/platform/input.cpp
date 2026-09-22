#include "platform/input.hpp"

#ifdef __vita__
#include <psp2/ctrl.h>
#endif

namespace wormhole {
namespace {
std::size_t index(Button b) { return static_cast<std::size_t>(b); }
}

void Input::beginFrame() { previous_ = state_; }

void Input::poll() {
#ifdef __vita__
    SceCtrlData pad{};
    if (sceCtrlPeekBufferPositive(0, &pad, 1) <= 0) return;
    setButton(Button::Left, pad.buttons & SCE_CTRL_LEFT);
    setButton(Button::Right, pad.buttons & SCE_CTRL_RIGHT);
    setButton(Button::Up, pad.buttons & SCE_CTRL_UP);
    setButton(Button::Down, pad.buttons & SCE_CTRL_DOWN);
    setButton(Button::Cross, pad.buttons & SCE_CTRL_CROSS);
    setButton(Button::Circle, pad.buttons & SCE_CTRL_CIRCLE);
    setButton(Button::Square, pad.buttons & SCE_CTRL_SQUARE);
    setButton(Button::Triangle, pad.buttons & SCE_CTRL_TRIANGLE);
    setButton(Button::L, pad.buttons & SCE_CTRL_LTRIGGER);
    setButton(Button::R, pad.buttons & SCE_CTRL_RTRIGGER);
    setButton(Button::Start, pad.buttons & SCE_CTRL_START);
    setButton(Button::Select, pad.buttons & SCE_CTRL_SELECT);
#endif
}

void Input::setButton(Button button, bool down) { state_[index(button)] = down; }
bool Input::down(Button button) const { return state_[index(button)]; }
bool Input::pressed(Button button) const { return state_[index(button)] && !previous_[index(button)]; }
}
