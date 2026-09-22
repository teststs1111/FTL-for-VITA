#include "platform/input.hpp"
#include <array>
namespace wormhole {
static std::array<bool, 12> state{};
static std::array<bool, 12> previous{};
static std::size_t index(Button b) { return static_cast<std::size_t>(b); }
void Input::beginFrame() { previous = state; }
void Input::setButton(Button button, bool down) { state[index(button)] = down; }
bool Input::down(Button button) const { return state[index(button)]; }
bool Input::pressed(Button button) const { return state[index(button)] && !previous[index(button)]; }
}
