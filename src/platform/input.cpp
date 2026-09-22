#include "platform/input.hpp"

namespace wormhole {
namespace {
std::size_t index(Button b) { return static_cast<std::size_t>(b); }
}

void Input::beginFrame() { previous_ = state_; }
void Input::setButton(Button button, bool down) { state_[index(button)] = down; }
bool Input::down(Button button) const { return state_[index(button)]; }
bool Input::pressed(Button button) const { return state_[index(button)] && !previous_[index(button)]; }
}
