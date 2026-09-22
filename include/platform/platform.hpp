#pragma once
#include <string>
namespace wormhole {
class Platform {
public:
    static std::string saveDirectory();
    static std::string ftlDatPath();
};
}
