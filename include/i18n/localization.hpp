#pragma once
#include <string>
#include <string_view>
#include <unordered_map>

namespace wormhole {

enum class Locale {
    English,
    Japanese,
};

class Localization {
public:
    Localization();

    void setLocale(Locale locale);
    Locale locale() const { return locale_; }

    std::string_view tr(std::string_view key) const;

private:
    Locale locale_{Locale::Japanese};
    std::unordered_map<std::string, std::string> japanese_;
    std::unordered_map<std::string, std::string> english_;
};

} // namespace wormhole
