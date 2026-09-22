#include "render/png_loader.hpp"
#include <algorithm>
#include <cstdio>
#include <png.h>

namespace wormhole {
namespace {

struct PngReader {
    const std::vector<std::uint8_t>* data{};
    std::size_t offset{0};
};

void readData(png_structp png, png_bytep dst, png_size_t length) {
    auto* reader = static_cast<PngReader*>(png_get_io_ptr(png));
    if (!reader || reader->offset + length > reader->data->size())
        png_error(png, "unexpected end of PNG");
    std::copy(reader->data->begin() + static_cast<std::ptrdiff_t>(reader->offset),
              reader->data->begin() + static_cast<std::ptrdiff_t>(reader->offset + length), dst);
    reader->offset += length;
}

}

bool decodePng(const std::vector<std::uint8_t>& data, RgbaImage& out) {
    out = {};
    if (data.size() < 8 || png_sig_cmp(const_cast<png_bytep>(data.data()), 0, 8) != 0)
        return false;

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png) return false;
    png_infop info = png_create_info_struct(png);
    if (!info) { png_destroy_read_struct(&png, nullptr, nullptr); return false; }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, nullptr);
        out = {};
        return false;
    }

    PngReader reader{&data, 0};
    png_set_read_fn(png, &reader, readData);
    png_read_info(png, info);

    png_uint_32 width = 0, height = 0;
    int bitDepth = 0, colorType = 0, interlace = 0, compression = 0, filter = 0;
    png_get_IHDR(png, info, &width, &height, &bitDepth, &colorType,
                 &interlace, &compression, &filter);
    if (width == 0 || height == 0 || width > 8192 || height > 8192)
        png_error(png, "invalid PNG dimensions");

    if (bitDepth == 16) png_set_strip_16(png);
    if (colorType == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png);
    if (colorType == PNG_COLOR_TYPE_GRAY && bitDepth < 8) png_set_expand_gray_1_2_4_to_8(png);
    if (png_get_valid(png, info, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png);
    if (colorType == PNG_COLOR_TYPE_RGB || colorType == PNG_COLOR_TYPE_GRAY ||
        colorType == PNG_COLOR_TYPE_PALETTE) png_set_filler(png, 0xff, PNG_FILLER_AFTER);
    if (colorType == PNG_COLOR_TYPE_GRAY || colorType == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png);

    png_read_update_info(png, info);
    const int channels = png_get_channels(png, info);
    if (channels != 4) png_error(png, "PNG did not normalize to RGBA");

    out.width = static_cast<int>(width);
    out.height = static_cast<int>(height);
    out.pixels.resize(static_cast<std::size_t>(width) * height * 4);
    std::vector<png_bytep> rows(height);
    for (png_uint_32 y = 0; y < height; ++y)
        rows[y] = out.pixels.data() + static_cast<std::size_t>(y) * width * 4;
    png_read_image(png, rows.data());
    png_read_end(png, nullptr);
    png_destroy_read_struct(&png, &info, nullptr);
    return true;
}

bool loadPng(const std::string& path, RgbaImage& out) {
    std::FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) return false;
    std::vector<std::uint8_t> data;
    std::uint8_t buffer[4096];
    while (const auto count = std::fread(buffer, 1, sizeof(buffer), file))
        data.insert(data.end(), buffer, buffer + count);
    const bool ok = std::ferror(file) == 0;
    std::fclose(file);
    return ok && decodePng(data, out);
}

}
