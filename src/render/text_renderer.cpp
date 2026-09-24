#include "render/text_renderer.hpp"
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#ifdef __vita__
#include <psp2/pvf.h>
#endif

namespace wormhole {

struct TextRenderer::Impl {
#ifdef __vita__
    ScePvfLibId lib{nullptr};
    ScePvfFontId japanese{nullptr};
    ScePvfFontId latin{nullptr};
    ScePvfError error{0};
#endif
    Texture texture{};
    std::string cachedText;
    int cachedSize{0};
    int width{0};
    int height{0};
};

TextRenderer::~TextRenderer() {
    delete impl_;
}

bool TextRenderer::init() {
    if (ready_) return true;
    delete impl_;
    impl_ = new Impl();

#ifdef __vita__
    ScePvfInitRec init{};
    init.maxNumFonts = 2;
    impl_->lib = scePvfNewLib(&init, &impl_->error);
    if (!impl_->lib) {
        delete impl_;
        impl_ = nullptr;
        return false;
    }

    impl_->japanese = scePvfOpenDefaultJapaneseFontOnSharedMemory(impl_->lib, &impl_->error);
    impl_->latin = scePvfOpenDefaultLatinFontOnSharedMemory(impl_->lib, &impl_->error);
    if (!impl_->japanese && !impl_->latin) {
        scePvfDoneLib(impl_->lib);
        delete impl_;
        impl_ = nullptr;
        return false;
    }
#endif

    ready_ = true;
    return true;
}

void TextRenderer::shutdown(Graphics& graphics) {
    if (!impl_) {
        ready_ = false;
        return;
    }

    graphics.destroyTexture(impl_->texture);

#ifdef __vita__
    if (impl_->japanese) scePvfClose(impl_->japanese);
    if (impl_->latin) scePvfClose(impl_->latin);
    if (impl_->lib) scePvfDoneLib(impl_->lib);
#endif

    delete impl_;
    impl_ = nullptr;
    ready_ = false;
}

#ifdef __vita__
static std::vector<std::uint16_t> utf8ToUtf16(std::string_view text) {
    std::vector<std::uint16_t> out;
    for (std::size_t i = 0; i < text.size();) {
        const auto c = static_cast<unsigned char>(text[i]);
        if (c < 0x80) {
            out.push_back(c);
            ++i;
        } else if ((c & 0xe0) == 0xc0 && i + 1 < text.size()) {
            const auto cp = static_cast<std::uint32_t>(c & 0x1f) << 6 |
                            (static_cast<unsigned char>(text[i + 1]) & 0x3f);
            out.push_back(static_cast<std::uint16_t>(cp));
            i += 2;
        } else if ((c & 0xf0) == 0xe0 && i + 2 < text.size()) {
            const auto cp = static_cast<std::uint32_t>(c & 0x0f) << 12 |
                            (static_cast<unsigned char>(text[i + 1]) & 0x3f) << 6 |
                            (static_cast<unsigned char>(text[i + 2]) & 0x3f);
            out.push_back(static_cast<std::uint16_t>(cp));
            i += 3;
        } else {
            // Current UI strings are BMP Japanese/Latin. Replace unsupported
            // sequences rather than walking past malformed UTF-8.
            out.push_back(static_cast<std::uint16_t>('?'));
            ++i;
        }
    }
    return out;
}

static ScePvfFontId selectFont(TextRenderer::Impl& impl, std::uint16_t codepoint) {
    if (codepoint >= 0x3000) return impl.japanese ? impl.japanese : impl.latin;
    return impl.latin ? impl.latin : impl.japanese;
}
#endif

void TextRenderer::draw(Graphics& graphics, std::string_view text, float x, float y,
                         float pixelSize, const Color& color) {
    if (!ready_ || !impl_ || text.empty()) return;

#ifdef __vita__
    const int size = std::max(1, static_cast<int>(pixelSize));
    if (impl_->cachedText != text || impl_->cachedSize != size) {
        graphics.destroyTexture(impl_->texture);

        const auto chars = utf8ToUtf16(text);
        int width = 0;
        int maxHeight = size;

        for (const auto code : chars) {
            auto font = selectFont(*impl_, code);
            if (!font) continue;
            if (scePvfSetCharSize(font, static_cast<float>(size), static_cast<float>(size)) < 0)
                continue;
            ScePvfCharInfo info{};
            if (scePvfGetCharInfo(font, code, &info) < 0) continue;
            width += std::max(1, static_cast<int>(info.glyphMetrics.horizontalAdvance64 / 64));
            maxHeight = std::max(maxHeight, static_cast<int>(info.bitmapHeight));
        }

        width = std::max(1, width);
        maxHeight = std::max(1, maxHeight);
        std::vector<std::uint8_t> rgba(static_cast<std::size_t>(width) * maxHeight * 4, 0);

        int penX = 0;
        for (const auto code : chars) {
            auto font = selectFont(*impl_, code);
            if (!font) continue;
            if (scePvfSetCharSize(font, static_cast<float>(size), static_cast<float>(size)) < 0)
                continue;

            ScePvfCharInfo info{};
            if (scePvfGetCharInfo(font, code, &info) < 0) continue;
            if (info.bitmapWidth == 0 || info.bitmapHeight == 0) {
                penX += std::max(1, static_cast<int>(info.glyphMetrics.horizontalAdvance64 / 64));
                continue;
            }

            std::vector<std::uint8_t> glyph(static_cast<std::size_t>(info.bitmapPitch) * info.bitmapHeight, 0);
            ScePvfUserImageBufferRec buffer{};
            buffer.pixelFormat = SCE_PVF_USERIMAGE_DIRECT8;
            buffer.xPos64 = 0;
            buffer.yPos64 = 0;
            buffer.rect.width = static_cast<std::uint16_t>(info.bitmapWidth);
            buffer.rect.height = static_cast<std::uint16_t>(info.bitmapHeight);
            buffer.bytesPerLine = static_cast<std::uint16_t>(info.bitmapPitch);
            buffer.buffer = glyph.data();

            if (scePvfGetCharGlyphImage(font, code, &buffer) < 0) {
                penX += std::max(1, static_cast<int>(info.glyphMetrics.horizontalAdvance64 / 64));
                continue;
            }

            const int glyphX = penX + info.bitmapLeft;
            const int glyphY = size - info.bitmapTop;
            for (std::uint32_t gy = 0; gy < info.bitmapHeight; ++gy) {
                for (std::uint32_t gx = 0; gx < info.bitmapWidth; ++gx) {
                    const int dx = glyphX + static_cast<int>(gx);
                    const int dy = glyphY + static_cast<int>(gy);
                    if (dx < 0 || dy < 0 || dx >= width || dy >= maxHeight) continue;
                    const auto alpha = glyph[gy * info.bitmapPitch + gx];
                    auto* dst = &rgba[(static_cast<std::size_t>(dy) * width + dx) * 4];
                    dst[0] = 255;
                    dst[1] = 255;
                    dst[2] = 255;
                    dst[3] = alpha;
                }
            }
            penX += std::max(1, static_cast<int>(info.glyphMetrics.horizontalAdvance64 / 64));
        }

        impl_->texture = graphics.createTexture(rgba, width, maxHeight);
        impl_->cachedText = std::string(text);
        impl_->cachedSize = size;
        impl_->width = width;
        impl_->height = maxHeight;
    }

    graphics.drawTexture(impl_->texture, x, y, static_cast<float>(impl_->width),
                         static_cast<float>(impl_->height), color);
#else
    (void)graphics;
    (void)text;
    (void)x;
    (void)y;
    (void)pixelSize;
    (void)color;
#endif
}

} // namespace wormhole
