#include "game/ship_runtime.hpp"
#include <cassert>

using namespace wormhole;

int main() {
    ShipRuntime ship;
    ship.valid = true;

    RoomBlueprint room0;
    room0.id = 0;
    room0.x = 10;
    room0.y = 20;
    room0.w = 4;
    room0.h = 3;

    RoomBlueprint room1;
    room1.id = 1;
    room1.x = 14;
    room1.y = 20;
    room1.w = 4;
    room1.h = 3;

    ship.content.layout.rooms = {room0, room1};

    assert(ship.roomAtLayoutPoint(10, 20) == 0);
    assert(ship.roomAtLayoutPoint(13, 22) == 0);
    assert(ship.roomAtLayoutPoint(14, 20) == 1);
    assert(ship.roomAtLayoutPoint(17, 22) == 1);
    assert(ship.roomAtLayoutPoint(9, 20) == -1);
    assert(ship.roomAtLayoutPoint(18, 20) == -1);
    assert(ship.roomAtLayoutPoint(13, 23) == -1);

    return 0;
}
