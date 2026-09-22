#pragma once
namespace wormhole {
enum class Button { Left, Right, Up, Down, Cross, Circle, Square, Triangle, L, R, Start, Select };
class Input {
public:
    void beginFrame();
    void setButton(Button button, bool down);
    bool down(Button button) const;
    bool pressed(Button button) const;
};
}
