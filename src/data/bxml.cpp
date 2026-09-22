#include "data/bxml.hpp"
#include <stdexcept>

namespace wormhole::bxml {
namespace {
class Reader {
public:
    explicit Reader(const std::vector<std::uint8_t>& data) : data_(data) {}

    std::uint32_t varint() {
        std::uint32_t value = 0;
        int shift = 0;
        while (true) {
            const auto b = byte();
            value |= std::uint32_t(b & 0x7f) << shift;
            if ((b & 0x80) == 0) return value;
            shift += 7;
            if (shift >= 32) throw std::runtime_error("BXML varint overflow");
        }
    }

    std::string string() {
        const auto id = varint();
        if (id != 0) {
            if (id - 1 >= strings_.size())
                throw std::runtime_error("BXML invalid string reference");
            return strings_[id - 1];
        }

        const auto length = varint();
        if (pos_ + length > data_.size())
            throw std::runtime_error("BXML truncated string");

        std::string result(reinterpret_cast<const char*>(data_.data() + pos_), length);
        pos_ += length;
        strings_.push_back(result);
        return result;
    }

    Node element() {
        Node node;
        node.name = string();

        const auto attributeCount = varint();
        for (std::uint32_t i = 0; i < attributeCount; ++i)
            node.attributes[string()] = string();

        while (true) {
            const auto type = byte();
            if (type == 2) break;
            if (type == 0) {
                node.text += string();
            } else if (type == 1) {
                node.children.push_back(element());
            } else {
                throw std::runtime_error("BXML invalid type ID");
            }
        }
        return node;
    }

private:
    std::uint8_t byte() {
        if (pos_ >= data_.size())
            throw std::runtime_error("BXML unexpected EOF");
        return data_[pos_++];
    }

    const std::vector<std::uint8_t>& data_;
    std::size_t pos_{0};
    std::vector<std::string> strings_;
};
}

Node read(const std::vector<std::uint8_t>& data) {
    return Reader(data).element();
}

}
