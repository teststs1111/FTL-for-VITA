#include "i18n/localization.hpp"
#include <cassert>
#include <string>

int main() {
    wormhole::Localization localization;

    assert(localization.locale() == wormhole::Locale::Japanese);
    assert(localization.tr("ui.start") == "開始");
    assert(localization.tr("ui.shields") == "シールド");

    localization.setLocale(wormhole::Locale::English);
    assert(localization.tr("ui.start") == "Start");
    assert(localization.tr("ui.shields") == "Shields");

    // Unknown keys must remain visible rather than producing an empty string.
    assert(localization.tr("missing.key") == "missing.key");
    return 0;
}
