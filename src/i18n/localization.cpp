#include "i18n/localization.hpp"
#include "data/bxml.hpp"

namespace wormhole {

Localization::Localization() {
    // Japanese is the default locale for the Vita release.
    japanese_ = {
        {"game.title", "FTL for PS Vita"},
        {"ui.start", "開始"},
        {"ui.continue", "続きから"},
        {"ui.new_game", "ニューゲーム"},
        {"ui.options", "オプション"},
        {"ui.quit", "終了"},
        {"ui.pause", "ポーズ"},
        {"ui.resume", "再開"},
        {"ui.crew", "クルー"},
        {"ui.weapons", "兵器"},
        {"ui.shields", "シールド"},
        {"ui.engines", "エンジン"},
        {"ui.oxygen", "酸素"},
        {"ui.medbay", "医療室"},
        {"ui.teleporter", "テレポーター"},
        {"ui.drones", "ドローン"},
        {"ui.hull", "船体"},
        {"ui.scrap", "スクラップ"},
        {"ui.fuel", "燃料"},
        {"ui.missiles", "ミサイル"},
        {"ui.drone_parts", "ドローンパーツ"},
        {"ui.jump", "ジャンプ"},
        {"ui.evasion", "回避"},
        {"ui.power", "電力"},
        {"ui.repair", "修理"},
        {"ui.fire", "発射"},
        {"ui.target", "照準"},
        {"ui.charge", "チャージ"},
        {"ui.pause_menu", "ポーズメニュー"},
        {"ui.game_over", "ゲームオーバー"},
        {"ui.victory", "勝利"},
        {"ui.defeat", "敗北"},
        {"ui.beacon", "ビーコン"},
        {"ui.sector", "セクター"},
        {"ui.event", "イベント"},
        {"ui.store", "ショップ"},
        {"ui.map", "マップ"},
        {"ui.save", "セーブ"},
        {"ui.load", "ロード"},
        {"ui.settings", "設定"},
        {"ui.touch", "タッチ操作"},
        {"ui.back", "戻る"},
        {"ui.confirm", "決定"},
        {"ui.cancel", "キャンセル"},
        {"combat.enemy", "敵艦"},
        {"combat.player", "自艦"},
        {"combat.escaped", "敵艦が逃走した"},
        {"combat.destroyed", "敵艦を撃破した"},
        {"combat.miss", "攻撃が外れた"},
        {"combat.shield_block", "シールドに阻まれた"},
    };

    english_ = {
        {"game.title", "FTL for PS Vita"},
        {"ui.start", "Start"},
        {"ui.continue", "Continue"},
        {"ui.new_game", "New Game"},
        {"ui.options", "Options"},
        {"ui.quit", "Quit"},
        {"ui.pause", "Pause"},
        {"ui.resume", "Resume"},
        {"ui.crew", "Crew"},
        {"ui.weapons", "Weapons"},
        {"ui.shields", "Shields"},
        {"ui.engines", "Engines"},
        {"ui.oxygen", "Oxygen"},
        {"ui.medbay", "Medbay"},
        {"ui.teleporter", "Teleporter"},
        {"ui.drones", "Drones"},
        {"ui.hull", "Hull"},
        {"ui.scrap", "Scrap"},
        {"ui.fuel", "Fuel"},
        {"ui.missiles", "Missiles"},
        {"ui.drone_parts", "Drone Parts"},
        {"ui.jump", "Jump"},
        {"ui.evasion", "Evasion"},
        {"ui.power", "Power"},
        {"ui.repair", "Repair"},
        {"ui.fire", "Fire"},
        {"ui.target", "Target"},
        {"ui.charge", "Charge"},
        {"ui.pause_menu", "Pause Menu"},
        {"ui.game_over", "Game Over"},
        {"ui.victory", "Victory"},
        {"ui.defeat", "Defeat"},
        {"ui.beacon", "Beacon"},
        {"ui.sector", "Sector"},
        {"ui.event", "Event"},
        {"ui.store", "Store"},
        {"ui.map", "Map"},
        {"ui.save", "Save"},
        {"ui.load", "Load"},
        {"ui.settings", "Settings"},
        {"ui.touch", "Touch Controls"},
        {"ui.back", "Back"},
        {"ui.confirm", "Confirm"},
        {"ui.cancel", "Cancel"},
        {"combat.enemy", "Enemy Ship"},
        {"combat.player", "Player Ship"},
        {"combat.escaped", "The enemy ship escaped"},
        {"combat.destroyed", "Enemy ship destroyed"},
        {"combat.miss", "Attack missed"},
        {"combat.shield_block", "Blocked by shields"},
    };
}

bool Localization::loadFtlTextXml(const std::vector<std::uint8_t>& data) {
    try {
        const auto root = bxml::read(data);
        if (root.name != "FTL") return false;
        std::size_t loaded = 0;
        for (const auto& child : root.children) {
            if (child.name != "text") continue;
            const auto nameIt = child.attributes.find("name");
            if (nameIt == child.attributes.end() || nameIt->second.empty()) continue;
            const auto langIt = child.attributes.find("language");
            if (langIt != child.attributes.end() && langIt->second != "ja") continue;
            japanese_[nameIt->second] = child.text;
            ++loaded;
        }
        return loaded != 0;
    } catch (...) {
        return false;
    }
}

void Localization::setLocale(Locale locale) {
    locale_ = locale;
}

std::string_view Localization::tr(std::string_view key) const {
    const auto& table = locale_ == Locale::Japanese ? japanese_ : english_;
    const auto it = table.find(std::string(key));
    if (it != table.end()) return it->second;

    // Missing translations deliberately fall back to English so a partially
    // localized screen never renders an empty label.
    const auto fallback = english_.find(std::string(key));
    if (fallback != english_.end()) return fallback->second;
    return key;
}

} // namespace wormhole
