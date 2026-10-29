#ifndef FCE360_SNAPSHOT_PNG_H
#define FCE360_SNAPSHOT_PNG_H

#include <cstring>
#include <vector>
#include <zlib.h>

struct SnapshotImage {
    unsigned char pixels[256 * 256];
    unsigned char palette[256 * 3];
    unsigned int height;
};

// The writer only reads this owned image, never the emulator's live framebuffer.
inline bool EncodeSnapshotPNG(const SnapshotImage& image, std::vector<unsigned char>& png)
{
    png.clear();
    if (image.height == 0 || image.height > 256) return false;
    std::vector<unsigned char> rows(image.height * 257);
    for (unsigned int y = 0; y < image.height; ++y) {
        rows[y * 257] = 0;
        std::memcpy(&rows[y * 257 + 1], image.pixels + y * 256, 256);
    }
    uLongf size = compressBound(static_cast<uLong>(rows.size()));
    std::vector<unsigned char> compressed(size);
    if (compress(&compressed[0], &size, &rows[0], static_cast<uLong>(rows.size())) != Z_OK)
        return false;
    compressed.resize(size);
    const unsigned char signature[] = {137, 80, 78, 71, 13, 10, 26, 10};
    png.insert(png.end(), signature, signature + 8);
    unsigned char header[13] = {0, 0, 1, 0, 0, 0, 0, 0, 8, 3, 0, 0, 0};
    header[6] = static_cast<unsigned char>(image.height >> 8);
    header[7] = static_cast<unsigned char>(image.height);
    const char* types[] = {"IHDR", "PLTE", "IDAT", "IEND"};
    const unsigned char* data[] = {header, image.palette, &compressed[0], NULL};
    const unsigned int lengths[] = {13, 768, static_cast<unsigned int>(compressed.size()), 0};
    for (int i = 0; i < 4; ++i) {
        unsigned int length = lengths[i];
        for (int shift = 24; shift >= 0; shift -= 8)
            png.push_back(static_cast<unsigned char>(length >> shift));
        size_t start = png.size();
        png.insert(png.end(), types[i], types[i] + 4);
        if (length) png.insert(png.end(), data[i], data[i] + length);
        uLong crc = crc32(0, &png[start], length + 4);
        for (int shift = 24; shift >= 0; shift -= 8)
            png.push_back(static_cast<unsigned char>(crc >> shift));
    }
    return true;
}

#endif
