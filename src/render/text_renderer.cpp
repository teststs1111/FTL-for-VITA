#include "render/text_renderer.hpp"
#include "platform/runtime_diagnostics.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
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
    struct CachedTexture {
        Texture texture{};
        std::string text;
        int size{0};
        int width{0};
        int height{0};
        std::uint64_t lastUse{0};
    };
    std::vector<CachedTexture> textures;
    std::uint64_t useCounter{0};
};

TextRenderer::~TextRenderer() { delete impl_; }

bool TextRenderer::init() {
    if (ready_) return true;
    delete impl_;
    impl_ = new Impl();
#ifdef __vita__
    RuntimeDiagnostics::checkpoint("pvf_init_begin");
    // PVF requires explicit allocator callbacks. libvita2d uses the same
    // pattern; leaving these callbacks null makes scePvfNewLib return
    // SCE_PVF_ERROR_ARG on real hardware.
    ScePvfInitRec init{};
    init.userData = nullptr;
    init.maxNumFonts = SCE_PVF_MAX_OPEN;
    init.cache = nullptr;
    init.reserved = nullptr;
    init.allocFunc = [](ScePvfPointer, unsigned int size) -> ScePvfPointer {
        return std::malloc((size + sizeof(int) - 1) / sizeof(int) * sizeof(int));
    };
    init.reallocFunc = [](ScePvfPointer, ScePvfPointer ptr, unsigned int size) -> ScePvfPointer {
        return std::realloc(ptr, (size + sizeof(int) - 1) / sizeof(int) * sizeof(int));
    };
    init.freeFunc = [](ScePvfPointer, ScePvfPointer ptr) {
        std::free(ptr);
    };
    impl_->lib = scePvfNewLib(&init, &impl_->error);
    RuntimeDiagnostics::checkpoint("pvf_newlib_complete",
        "lib=" + std::to_string(reinterpret_cast<std::uintptr_t>(impl_->lib)) +
        " error=" + std::to_string(static_cast<int>(impl_->error)));
    if (!impl_->lib) { delete impl_; impl_ = nullptr; return false; }
    RuntimeDiagnostics::checkpoint("pvf_open_japanese_begin");
    impl_->japanese = scePvfOpenDefaultJapaneseFontOnSharedMemory(impl_->lib, &impl_->error);
    RuntimeDiagnostics::checkpoint("pvf_open_japanese_complete",
        "font=" + std::to_string(reinterpret_cast<std::uintptr_t>(impl_->japanese)) +
        " error=" + std::to_string(static_cast<int>(impl_->error)));
    RuntimeDiagnostics::checkpoint("pvf_open_latin_begin");
    impl_->latin = scePvfOpenDefaultLatinFontOnSharedMemory(impl_->lib, &impl_->error);
    RuntimeDiagnostics::checkpoint("pvf_open_latin_complete",
        "font=" + std::to_string(reinterpret_cast<std::uintptr_t>(impl_->latin)) +
        " error=" + std::to_string(static_cast<int>(impl_->error)));
    if (!impl_->japanese && !impl_->latin) {
        scePvfDoneLib(impl_->lib);
        delete impl_; impl_ = nullptr; return false;
    }
#endif
    ready_ = true;
    return true;
}

void TextRenderer::shutdown(Graphics& graphics) {
    if (!impl_) { ready_ = false; return; }
    for (auto& cached : impl_->textures) graphics.destroyTexture(cached.texture);
    impl_->textures.clear();
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
        if (c < 0x80) { out.push_back(c); ++i; }
        else if ((c & 0xe0) == 0xc0 && i + 1 < text.size()) {
            const auto cp = static_cast<std::uint32_t>(c & 0x1f) << 6 |
                            (static_cast<unsigned char>(text[i + 1]) & 0x3f);
            out.push_back(static_cast<std::uint16_t>(cp)); i += 2;
        } else if ((c & 0xf0) == 0xe0 && i + 2 < text.size()) {
            const auto cp = static_cast<std::uint32_t>(c & 0x0f) << 12 |
                            (static_cast<unsigned char>(text[i + 1]) & 0x3f) << 6 |
                            (static_cast<unsigned char>(text[i + 2]) & 0x3f);
            out.push_back(static_cast<std::uint16_t>(cp)); i += 3;
        } else { out.push_back(static_cast<std::uint16_t>('?')); ++i; }
    }
    return out;
}
static ScePvfFontId selectFont(ScePvfFontId japanese, ScePvfFontId latin, std::uint16_t codepoint) {
    if (codepoint >= 0x3000) return japanese ? japanese : latin;
    return latin ? latin : japanese;
}
#endif

void TextRenderer::draw(Graphics& graphics, std::string_view text, float x, float y,
                         float pixelSize, const Color& color) {
    if (!ready_ || !impl_ || text.empty()) return;
#ifdef __vita__
    const int size = std::max(1, static_cast<int>(pixelSize));
    Impl::CachedTexture* cached = nullptr;
    for (auto& item : impl_->textures) {
        if (item.size == size && item.text == text) {
            item.lastUse = ++impl_->useCounter;
            cached = &item;
            break;
        }
    }
    if (!cached) {
        RuntimeDiagnostics::checkpoint("text_texture_build_begin",
            "chars=" + std::to_string(text.size()) + " size=" + std::to_string(size));
        const auto chars = utf8ToUtf16(text);
        int width = 0, maxHeight = size;
        for (const auto code : chars) {
            auto font = selectFont(impl_->japanese, impl_->latin, code);
            if (!font || scePvfSetCharSize(font, static_cast<float>(size), static_cast<float>(size)) < 0) continue;
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
            auto font = selectFont(impl_->japanese, impl_->latin, code);
            if (!font || scePvfSetCharSize(font, static_cast<float>(size), static_cast<float>(size)) < 0) continue;
            ScePvfCharInfo info{};
            if (scePvfGetCharInfo(font, code, &info) < 0) continue;
            if (info.bitmapWidth == 0 || info.bitmapHeight == 0) {
                penX += std::max(1, static_cast<int>(info.glyphMetrics.horizontalAdvance64 / 64));
                continue;
            }
            std::vector<std::uint8_t> glyph(static_cast<std::size_t>(info.bitmapPitch) * info.bitmapHeight, 0);
            ScePvfUserImageBufferRec buffer{};
            buffer.pixelFormat = SCE_PVF_USERIMAGE_DIRECT8;
            buffer.xPos64 = 0; buffer.yPos64 = 0;
            buffer.rect.width = static_cast<std::uint16_t>(info.bitmapWidth);
            buffer.rect.height = static_cast<std::uint16_t>(info.bitmapHeight);
            buffer.bytesPerLine = static_cast<std::uint16_t>(info.bitmapPitch);
            buffer.buffer = glyph.data();
            if (scePvfGetCharGlyphImage(font, code, &buffer) < 0) {
                penX += std::max(1, static_cast<int>(info.glyphMetrics.horizontalAdvance64 / 64));
                continue;
            }
            const int glyphX = penX + info.bitmapLeft, glyphY = size - info.bitmapTop;
            for (std::uint32_t gy = 0; gy < info.bitmapHeight; ++gy)
                for (std::uint32_t gx = 0; gx < info.bitmapWidth; ++gx) {
                    const int dx = glyphX + static_cast<int>(gx), dy = glyphY + static_cast<int>(gy);
                    if (dx < 0 || dy < 0 || dx >= width || dy >= maxHeight) continue;
                    const auto alpha = glyph[gy * info.bitmapPitch + gx];
                    auto* dst = &rgba[(static_cast<std::size_t>(dy) * width + dx) * 4];
                    dst[0] = dst[1] = dst[2] = 255; dst[3] = alpha;
                }
            penX += std::max(1, static_cast<int>(info.glyphMetrics.horizontalAdvance64 / 64));
        }
        RuntimeDiagnostics::checkpoint("text_texture_upload_begin",
            "width=" + std::to_string(width) + " height=" + std::to_string(maxHeight));
        Impl::CachedTexture entry;
        entry.texture = graphics.createTexture(rgba, width, maxHeight);
        entry.text = std::string(text);
        entry.size = size;
        entry.width = width; entry.height = maxHeight;
        entry.lastUse = ++impl_->useCounter;
        constexpr std::size_t maxCachedTextures = 16;
        if (impl_->textures.size() >= maxCachedTextures) {
            auto victim = std::min_element(impl_->textures.begin(), impl_->textures.end(),
                [](const auto& a, const auto& b) { return a.lastUse < b.lastUse; });
            graphics.destroyTexture(victim->texture);
            *victim = std::move(entry);
            cached = &*victim;
        } else {
            impl_->textures.push_back(std::move(entry));
            cached = &impl_->textures.back();
        }
        RuntimeDiagnostics::checkpoint("text_texture_upload_complete",
            "valid=" + std::to_string(cached->texture.valid() ? 1 : 0) +
            " cache=" + std::to_string(impl_->textures.size()));
    }
    graphics.drawTexture(cached->texture, x, y, static_cast<float>(cached->width),
                         static_cast<float>(cached->height), color);
#else
    (void)graphics; (void)text; (void)x; (void)y; (void)pixelSize; (void)color;
#endif
}

} // namespace wormhole
