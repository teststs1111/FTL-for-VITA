#include "i18n/localization.hpp"
#include <cassert>
#include <string>
#include <vector>

int main() {
    wormhole::Localization localization;

    assert(localization.locale() == wormhole::Locale::Japanese);
    assert(localization.tr("ui.start") == "開始");
    assert(localization.tr("ui.shields") == "シールド");

    const std::string xml = R"(<?xml version="1.0"?><FTL><text name="event_TEST_text" language="ja">テスト&amp;確認</text><text name="event_TEST_choice" language="ja">選択</text></FTL>)";
    assert(localization.loadFtlTextXml(std::vector<std::uint8_t>(xml.begin(), xml.end())));
    localization.setLocale(wormhole::Locale::Japanese);
    assert(localization.tr("event_TEST_text") == "テスト&確認");
    assert(localization.tr("event_TEST_choice") == "選択");

    localization.setLocale(wormhole::Locale::English);
    assert(localization.tr("ui.start") == "Start");
    assert(localization.tr("ui.shields") == "Shields");

    // Unknown keys must remain visible rather than producing an empty string.
    assert(localization.tr("missing.key") == "missing.key");
    return 0;
}
