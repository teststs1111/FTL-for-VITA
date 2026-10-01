#pragma once
#include "i18n/localization.hpp"
#include "audio/audio.hpp"
#include <memory>

namespace wormhole { class Input; }
namespace wormhole {
enum class Difficulty { Easy, Normal, Hard };
class GameState;
class Graphics;
class MainGame {
public:
    MainGame();
    ~MainGame();
    void init(Graphics& graphics, Input& input, const char* archivePath = "ux0:data/wormhole/ftl.dat");
    void update(float dt);
    void render();
    void shutdown();
    void setState(std::unique_ptr<GameState> state);

    // Japanese is the default Vita locale; the renderer/UI can use this
    // catalog without coupling game logic to translated strings.
    const Localization& localization() const { return localization_; }
    void setLocale(Locale locale) { localization_.setLocale(locale); }
    void setDifficulty(Difficulty difficulty);

private:
    std::unique_ptr<GameState> state_;
    Localization localization_;
    bool initialized_{false};
    Difficulty difficulty_{Difficulty::Normal};
    Audio audio_;
    bool audioInitialized_{false};
};
}
