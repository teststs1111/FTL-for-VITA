#include "data/bxml.hpp"

#include <cctype>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace wormhole::bxml {
namespace {

class XmlReader {
public:
    explicit XmlReader(const std::vector<std::uint8_t>& data)
        : text_(reinterpret_cast<const char*>(data.data()), data.size()) {}

    Node readDocument() {
        skipBom();
        skipWhitespace();
        if (startsWith("<?")) skipProcessingInstruction();
        skipMisc();
        Node root = element();
        skipMisc();
        skipWhitespace();
        if (pos_ != text_.size())
            throw std::runtime_error("XML trailing data");
        return root;
    }

private:
    void skipBom() {
        if (text_.size() >= 3 &&
            static_cast<unsigned char>(text_[0]) == 0xef &&
            static_cast<unsigned char>(text_[1]) == 0xbb &&
            static_cast<unsigned char>(text_[2]) == 0xbf)
            pos_ = 3;
    }

    bool startsWith(const char* s) const {
        std::size_t i = 0;
        while (s[i] != '\0') {
            if (pos_ + i >= text_.size() || text_[pos_ + i] != s[i]) return false;
            ++i;
        }
        return true;
    }

    void skipWhitespace() {
        while (pos_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[pos_]))) ++pos_;
    }

    void skipMisc() {
        while (true) {
            skipWhitespace();
            if (startsWith("<!--")) { skipComment(); continue; }
            if (startsWith("<?")) { skipProcessingInstruction(); continue; }
            break;
        }
    }

    void skipComment() {
        const auto end = text_.find("-->", pos_ + 4);
        if (end == std::string::npos) throw std::runtime_error("XML unterminated comment");
        pos_ = end + 3;
    }

    void skipProcessingInstruction() {
        const auto end = text_.find("?>", pos_ + 2);
        if (end == std::string::npos) throw std::runtime_error("XML unterminated processing instruction");
        pos_ = end + 2;
    }

    std::string name() {
        const auto begin = pos_;
        while (pos_ < text_.size()) {
            const unsigned char c = static_cast<unsigned char>(text_[pos_]);
            if (std::isalnum(c) || c == '_' || c == '-' || c == ':' || c == '.')
                ++pos_;
            else
                break;
        }
        if (begin == pos_) throw std::runtime_error("XML expected name");
        return text_.substr(begin, pos_ - begin);
    }

    void expect(char c) {
        if (pos_ >= text_.size() || text_[pos_] != c)
            throw std::runtime_error("XML unexpected character");
        ++pos_;
    }

    std::string quoted() {
        if (pos_ >= text_.size() || (text_[pos_] != '\'' && text_[pos_] != '"'))
            throw std::runtime_error("XML expected quoted attribute");
        const char quote = text_[pos_++];
        std::string raw;
        while (pos_ < text_.size() && text_[pos_] != quote) raw.push_back(text_[pos_++]);
        if (pos_ >= text_.size()) throw std::runtime_error("XML unterminated attribute");
        ++pos_;
        return decodeEntities(raw);
    }

    static std::string decodeEntities(const std::string& raw) {
        std::string out;
        out.reserve(raw.size());
        for (std::size_t i = 0; i < raw.size(); ++i) {
            if (raw[i] != '&') { out.push_back(raw[i]); continue; }
            const auto semi = raw.find(';', i + 1);
            if (semi == std::string::npos) throw std::runtime_error("XML unterminated entity");
            const auto e = raw.substr(i + 1, semi - i - 1);
            if (e == "amp") out += '&';
            else if (e == "lt") out += '<';
            else if (e == "gt") out += '>';
            else if (e == "quot") out += '"';
            else if (e == "apos") out += '\'';
            else if (!e.empty() && e[0] == '#') {
                unsigned long value = 0;
                try {
                    value = (e.size() > 1 && (e[1] == 'x' || e[1] == 'X'))
                        ? std::stoul(e.substr(2), nullptr, 16)
                        : std::stoul(e.substr(1), nullptr, 10);
                } catch (...) {
                    throw std::runtime_error("XML invalid character entity");
                }
                if (value <= 0x7f) out.push_back(static_cast<char>(value));
                else if (value <= 0x7ff) {
                    out.push_back(static_cast<char>(0xc0 | (value >> 6)));
                    out.push_back(static_cast<char>(0x80 | (value & 0x3f)));
                } else if (value <= 0xffff) {
                    out.push_back(static_cast<char>(0xe0 | (value >> 12)));
                    out.push_back(static_cast<char>(0x80 | ((value >> 6) & 0x3f)));
                    out.push_back(static_cast<char>(0x80 | (value & 0x3f)));
                } else if (value <= 0x10ffff) {
                    out.push_back(static_cast<char>(0xf0 | (value >> 18)));
                    out.push_back(static_cast<char>(0x80 | ((value >> 12) & 0x3f)));
                    out.push_back(static_cast<char>(0x80 | ((value >> 6) & 0x3f)));
                    out.push_back(static_cast<char>(0x80 | (value & 0x3f)));
                } else throw std::runtime_error("XML invalid character entity");
            } else {
                throw std::runtime_error("XML unknown entity");
            }
            i = semi;
        }
        return out;
    }

    Node element() {
        expect('<');
        if (startsWith("!")) throw std::runtime_error("XML unexpected declaration");
        Node node;
        node.name = name();

        while (true) {
            skipWhitespace();
            if (startsWith("/>")) {
                pos_ += 2;
                return node;
            }
            if (pos_ < text_.size() && text_[pos_] == '>') {
                ++pos_;
                break;
            }
            const auto key = name();
            skipWhitespace();
            expect('=');
            skipWhitespace();
            node.attributes[key] = quoted();
        }

        while (true) {
            if (startsWith("</")) {
                pos_ += 2;
                const auto closeName = name();
                if (closeName != node.name) throw std::runtime_error("XML mismatched closing tag");
                skipWhitespace();
                expect('>');
                return node;
            }
            if (startsWith("<!--")) { skipComment(); continue; }
            if (startsWith("<?")) { skipProcessingInstruction(); continue; }
            if (startsWith("<![CDATA[")) {
                const auto begin = pos_ + 9;
                const auto end = text_.find("]]>", begin);
                if (end == std::string::npos) throw std::runtime_error("XML unterminated CDATA");
                node.text += text_.substr(begin, end - begin);
                pos_ = end + 3;
                continue;
            }
            if (pos_ < text_.size() && text_[pos_] == '<') {
                node.children.push_back(element());
                continue;
            }
            const auto begin = pos_;
            while (pos_ < text_.size() && text_[pos_] != '<') ++pos_;
            node.text += decodeEntities(text_.substr(begin, pos_ - begin));
        }
    }

    std::string text_;
    std::size_t pos_{0};
};

}

Node read(const std::vector<std::uint8_t>& data) {
    if (data.empty()) throw std::runtime_error("XML empty document");
    return XmlReader(data).readDocument();
}

}
