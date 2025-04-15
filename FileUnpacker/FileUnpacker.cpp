// FileUnpacker.cpp : Defines the entry point for the application.
//

#include "FileUnpacker.h"

using namespace std;
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <iomanip>
#include <cstring>
#include <filesystem>
#include <cstdint>
#include <limits>
#include <map>
#include <cmath>

#pragma pack(push, 1)
struct BMPHeader {
    uint16_t signature;     // 'BM'
    uint32_t fileSize;      // Size of the BMP file in bytes
    uint16_t reserved1;     // Reserved, must be 0
    uint16_t reserved2;     // Reserved, must be 0
    uint32_t dataOffset;    // Offset to the start of image data
};

struct DIBHeader {
    uint32_t headerSize;    // Size of this header (40 bytes)
    int32_t width;          // Width of the image
    int32_t height;         // Height of the image
    uint16_t planes;        // Number of color planes (must be 1)
    uint16_t bitsPerPixel;  // Bits per pixel (24 for RGB)
    uint32_t compression;   // Compression method (0 for none)
    uint32_t imageSize;     // Size of the image data
    int32_t xPixelsPerM;    // Horizontal resolution (pixels per meter)
    int32_t yPixelsPerM;    // Vertical resolution (pixels per meter)
    uint32_t colorsUsed;    // Number of colors in the palette
    uint32_t importantColors; // Number of important colors
};
#pragma pack(pop)

enum class FileFormat {
    WAV,
    D3GR
};

struct FormatInfo {
    std::string name;
    std::string extension;
    std::string folderName;
};

const std::map<FileFormat, FormatInfo> formatInfoMap = {
    {FileFormat::WAV, {"WAV Audio", "wav", "extracted_wav"}},
    {FileFormat::D3GR, {"D3GR (Sanitarium Graphic Resource file)", "d3gr", "extracted_gr"}}
};

void printHexBuffer(const char* data, size_t size, size_t position) {
    std::cout << "Position " << position << " Hex: ";
    for (size_t i = 0; i < std::min(size_t(16), size); ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(static_cast<unsigned char>(data[i])) << " ";
    }
    std::cout << std::dec << std::endl;
}

// Extract WAV file size from header
uint32_t getWavSize(const char* data) {
    // WAV file size is at bytes 4-7 (little endian) + 8 bytes for header
    uint32_t size = static_cast<uint8_t>(data[4]) |
        (static_cast<uint8_t>(data[5]) << 8) |
        (static_cast<uint8_t>(data[6]) << 16) |
        (static_cast<uint8_t>(data[7]) << 24);
    return size + 8;
}

uint32_t getGraphicsResourceSize(const char* data) {
    // Frame count is at offset 0x18 (2-byte little endian)
    uint16_t frameCount = static_cast<uint16_t>(
        static_cast<uint8_t>(data[0x18]) |
        (static_cast<uint8_t>(data[0x19]) << 8)
        );

    uint32_t offsetsEndPosition = 0x1C + (frameCount * 4);
    uint32_t lastFrameOffset = 0;
    size_t lastOffsetPos = 0x1C + ((frameCount - 1) * 4);

    lastFrameOffset = static_cast<uint8_t>(data[lastOffsetPos]) |
        (static_cast<uint8_t>(data[lastOffsetPos + 1]) << 8) |
        (static_cast<uint8_t>(data[lastOffsetPos + 2]) << 16) |
        (static_cast<uint8_t>(data[lastOffsetPos + 3]) << 24);

    uint32_t lastFramePosition = offsetsEndPosition + lastFrameOffset;
    uint16_t lastFrameWidth = static_cast<uint16_t>(
        static_cast<uint8_t>(data[lastFramePosition + 0x0E]) |
        (static_cast<uint8_t>(data[lastFramePosition + 0x0E + 1]) << 8)
        );

    uint16_t lastFrameHeight = static_cast<uint16_t>(
        static_cast<uint8_t>(data[lastFramePosition + 0x0C]) |
        (static_cast<uint8_t>(data[lastFramePosition + 0x0C + 1]) << 8)
        );

    // frame's data
    uint32_t lastFrameDataSize = lastFrameWidth * lastFrameHeight;
    // Total size is the position of the last frame + its header (0x10) + its data size
    uint32_t totalSize = lastFramePosition + 0x10 + lastFrameDataSize;
    return totalSize;
}

// Function to search for WAV header pattern
size_t findWavHeader(const char* buffer, size_t bufferSize) {
    // Look for the pattern RIFF____WAVEfmt (where ____ is any 4 bytes)
    for (size_t i = 0; i <= bufferSize - 12; ++i) {
        if ((unsigned char)buffer[i] == 0x52 && // 'R'
            (unsigned char)buffer[i + 1] == 0x49 && // 'I'
            (unsigned char)buffer[i + 2] == 0x46 && // 'F'
            (unsigned char)buffer[i + 3] == 0x46 && // 'F'
            // Skip size bytes (4-7)
            (unsigned char)buffer[i + 8] == 0x57 && // 'W'
            (unsigned char)buffer[i + 9] == 0x41 && // 'A'
            (unsigned char)buffer[i + 10] == 0x56 && // 'V'
            (unsigned char)buffer[i + 11] == 0x45) { // 'E'
            return i;
        }
    }
    return SIZE_MAX; // Not found
}

size_t findGraphicsResourceHeader(const char* buffer, size_t bufferSize) {
    // Signature is D3GR
    for (size_t i = 0; i <= bufferSize - 4; ++i) {
        if (buffer[i] == 'D' &&
            buffer[i + 1] == '3' &&
            buffer[i + 2] == 'G' &&
            buffer[i + 3] == 'R') {
            return i;
        }
    }
    return SIZE_MAX; // Not found
}

// TODO: move the palettes to a separate file and read them from there
// For now they're going to live here until I can find all of them and map them correctly
uint8_t paletteDataRes006[256][4] = {
    {0x00, 0x00, 0x00, 0xFF}, // 0
    {0xFC, 0xFC, 0xFC, 0xFF}, // 1
    {0xFC, 0xF4, 0xC4, 0xFF}, // 2
    {0xD4, 0xFC, 0xFC, 0xFF}, // 3
    {0xFC, 0xF4, 0x94, 0xFF}, // 4
    {0xE8, 0xE8, 0xE8, 0xFF}, // 5
    {0xE8, 0xE0, 0xFC, 0xFF}, // 6
    {0xFC, 0xF0, 0x1C, 0xFF}, // 7
    {0xFC, 0xF0, 0x18, 0xFF}, // 8
    {0xB8, 0xE0, 0xFC, 0xFF}, // 9
    {0xD4, 0xD4, 0xD4, 0xFF}, // 10
    {0xA8, 0xE4, 0xFC, 0xFF}, // 11
    {0xEC, 0xD0, 0xA0, 0xFF}, // 12
    {0xB8, 0xF0, 0x8C, 0xFF}, // 13
    {0xE4, 0xDC, 0x58, 0xFF}, // 14
    {0xD0, 0xD0, 0xD0, 0xFF}, // 15
    {0xB0, 0xE4, 0x84, 0xFF}, // 16
    {0xC4, 0xC4, 0xC4, 0xFF}, // 17
    {0xE0, 0xD0, 0x1C, 0xFF}, // 18
    {0xE8, 0xC8, 0x2C, 0xFF}, // 19
    {0xEC, 0xC8, 0x1C, 0xFF}, // 20
    {0xE4, 0xB8, 0x80, 0xFF}, // 21
    {0xC8, 0xB8, 0xB4, 0xFF}, // 22
    {0xA8, 0xD4, 0x74, 0xFF}, // 23
    {0xE0, 0xAC, 0x94, 0xFF}, // 24
    {0x9C, 0xD0, 0x70, 0xFF}, // 25
    {0xC4, 0xAC, 0xA4, 0xFF}, // 26
    {0xB8, 0xB8, 0xB8, 0xFF}, // 27
    {0x80, 0xBC, 0xFC, 0xFF}, // 28
    {0xC4, 0xA8, 0x9C, 0xFF}, // 29
    {0xC0, 0xBC, 0x34, 0xFF}, // 30
    {0xCC, 0x9C, 0x84, 0xFF}, // 31
    {0xD4, 0xAC, 0x1C, 0xFF}, // 32
    {0xDC, 0xA8, 0x28, 0xFF}, // 33
    {0xDC, 0xA4, 0x24, 0xFF}, // 34
    {0x8C, 0xC0, 0x64, 0xFF}, // 35
    {0xD8, 0xA4, 0x24, 0xFF}, // 36
    {0xB8, 0xA0, 0x98, 0xFF}, // 37
    {0xA4, 0xA4, 0xA4, 0xFF}, // 38
    {0xDC, 0x84, 0x6C, 0xFF}, // 39
    {0x64, 0xC4, 0x30, 0xFF}, // 40
    {0xC0, 0x90, 0x78, 0xFF}, // 41
    {0x80, 0xB4, 0x5C, 0xFF}, // 42
    {0xA8, 0x90, 0x88, 0xFF}, // 43
    {0xC8, 0x8C, 0x28, 0xFF}, // 44
    {0xA8, 0x8C, 0x9C, 0xFF}, // 45
    {0xC8, 0x88, 0x2C, 0xFF}, // 46
    {0xB4, 0x98, 0x34, 0xFF}, // 47
    {0xB0, 0x90, 0x44, 0xFF}, // 48
    {0x94, 0x94, 0x94, 0xFF}, // 49
    {0x94, 0x94, 0x80, 0xFF}, // 50
    {0x88, 0x84, 0xC4, 0xFF}, // 51
    {0xB0, 0x84, 0x68, 0xFF}, // 52
    {0xBC, 0x7C, 0x64, 0xFF}, // 53
    {0xB8, 0x70, 0x9C, 0xFF}, // 54
    {0x64, 0xAC, 0x34, 0xFF}, // 55
    {0xB4, 0x84, 0x24, 0xFF}, // 56
    {0x84, 0x80, 0xB8, 0xFF}, // 57
    {0x98, 0x7C, 0x90, 0xFF}, // 58
    {0x70, 0x9C, 0x40, 0xFF}, // 59
    {0x94, 0x80, 0x78, 0xFF}, // 60
    {0xBC, 0x74, 0x2C, 0xFF}, // 61
    {0x80, 0x80, 0x80, 0xFF}, // 62
    {0xA0, 0x74, 0x5C, 0xFF}, // 63
    {0xA8, 0x68, 0x88, 0xFF}, // 64
    {0x78, 0x78, 0xA8, 0xFF}, // 65
    {0xA0, 0x74, 0x38, 0xFF}, // 66
    {0xC0, 0x58, 0x40, 0xFF}, // 67
    {0x5C, 0x94, 0x34, 0xFF}, // 68
    {0x88, 0x6C, 0x80, 0xFF}, // 69
    {0x84, 0x70, 0x68, 0xFF}, // 70
    {0xA4, 0x68, 0x28, 0xFF}, // 71
    {0x70, 0x70, 0x9C, 0xFF}, // 72
    {0x68, 0x78, 0x74, 0xFF}, // 73
    {0x9C, 0x5C, 0x7C, 0xFF}, // 74
    {0xA8, 0x60, 0x2C, 0xFF}, // 75
    {0xA8, 0x60, 0x20, 0xFF}, // 76
    {0x94, 0x64, 0x50, 0xFF}, // 77
    {0x70, 0x70, 0x70, 0xFF}, // 78
    {0x5C, 0x80, 0x38, 0xFF}, // 79
    {0x8C, 0x5C, 0x6C, 0xFF}, // 80
    {0x80, 0x60, 0x74, 0xFF}, // 81
    {0x68, 0x64, 0x90, 0xFF}, // 82
    {0x9C, 0x60, 0x1C, 0xFF}, // 83
    {0x40, 0x8C, 0x1C, 0xFF}, // 84
    {0x84, 0x68, 0x20, 0xFF}, // 85
    {0x98, 0x58, 0x50, 0xFF}, // 86
    {0x84, 0x68, 0x20, 0xFF}, // 87
    {0x60, 0x70, 0x64, 0xFF}, // 88
    {0x74, 0x60, 0x58, 0xFF}, // 89
    {0x78, 0x5C, 0x64, 0xFF}, // 90
    {0x48, 0x7C, 0x24, 0xFF}, // 91
    {0xD4, 0x2C, 0x24, 0xFF}, // 92
    {0x90, 0x58, 0x18, 0xFF}, // 93
    {0x80, 0x58, 0x44, 0xFF}, // 94
    {0x88, 0x54, 0x34, 0xFF}, // 95
    {0x54, 0x68, 0x5C, 0xFF}, // 96
    {0x78, 0x60, 0x1C, 0xFF}, // 97
    {0x5C, 0x5C, 0x80, 0xFF}, // 98
    {0x98, 0x48, 0x2C, 0xFF}, // 99
    {0x5C, 0x5C, 0x80, 0xFF}, // 100
    {0x34, 0x80, 0x14, 0xFF}, // 101
    {0x74, 0x60, 0x0C, 0xFF}, // 102
    {0x5C, 0x5C, 0x5C, 0xFF}, // 103
    {0x58, 0x54, 0x94, 0xFF}, // 104
    {0x80, 0x4C, 0x64, 0xFF}, // 105
    {0x94, 0x44, 0x2C, 0xFF}, // 106
    {0x70, 0x58, 0x1C, 0xFF}, // 107
    {0x80, 0x54, 0x10, 0xFF}, // 108
    {0x70, 0x58, 0x1C, 0xFF}, // 109
    {0x64, 0x54, 0x50, 0xFF}, // 110
    {0x54, 0x54, 0x74, 0xFF}, // 111
    {0x84, 0x48, 0x24, 0xFF}, // 112
    {0x68, 0x4C, 0x60, 0xFF}, // 113
    {0xA8, 0x30, 0x24, 0xFF}, // 114
    {0x74, 0x4C, 0x3C, 0xFF}, // 115
    {0x50, 0x48, 0x9C, 0xFF}, // 116
    {0x78, 0x50, 0x10, 0xFF}, // 117
    {0x6C, 0x4C, 0x58, 0xFF}, // 118
    {0x5C, 0x54, 0x44, 0xFF}, // 119
    {0x84, 0x44, 0x2C, 0xFF}, // 120
    {0x4C, 0x58, 0x54, 0xFF}, // 121
    {0x70, 0x48, 0x54, 0xFF}, // 122
    {0x5C, 0x5C, 0x0C, 0xFF}, // 123
    {0x84, 0x3C, 0x2C, 0xFF}, // 124
    {0x30, 0x6C, 0x14, 0xFF}, // 125
    {0x68, 0x54, 0x0C, 0xFF}, // 126
    {0xAC, 0x28, 0x1C, 0xFF}, // 127
    {0x68, 0x4C, 0x18, 0xFF}, // 128
    {0xFC, 0x00, 0x00, 0xFF}, // 129
    {0x44, 0x54, 0x48, 0xFF}, // 130
    {0x6C, 0x4C, 0x0C, 0xFF}, // 131
    {0x4C, 0x48, 0x68, 0xFF}, // 132
    {0x74, 0x40, 0x2C, 0xFF}, // 133
    {0x54, 0x54, 0x08, 0xFF}, // 134
    {0x48, 0x48, 0x68, 0xFF}, // 135
    {0x68, 0x3C, 0x48, 0xFF}, // 136
    {0x58, 0x44, 0x50, 0xFF}, // 137
    {0x50, 0x48, 0x44, 0xFF}, // 138
    {0x5C, 0x48, 0x18, 0xFF}, // 139
    {0x44, 0x3C, 0x80, 0xFF}, // 140
    {0x60, 0x40, 0x28, 0xFF}, // 141
    {0x74, 0x34, 0x24, 0xFF}, // 142
    {0x28, 0x60, 0x10, 0xFF}, // 143
    {0x3C, 0x4C, 0x40, 0xFF}, // 144
    {0x78, 0x30, 0x14, 0xFF}, // 145
    {0x4C, 0x4C, 0x08, 0xFF}, // 146
    {0x5C, 0x40, 0x08, 0xFF}, // 147
    {0x60, 0x3C, 0x14, 0xFF}, // 148
    {0x54, 0x40, 0x14, 0xFF}, // 149
    {0x50, 0x3C, 0x38, 0xFF}, // 150
    {0x54, 0x40, 0x18, 0xFF}, // 151
    {0xD0, 0x00, 0x00, 0xFF}, // 152
    {0x40, 0x3C, 0x58, 0xFF}, // 153
    {0x58, 0x34, 0x40, 0xFF}, // 154
    {0x68, 0x30, 0x1C, 0xFF}, // 155
    {0x3C, 0x34, 0x70, 0xFF}, // 156
    {0x50, 0x40, 0x10, 0xFF}, // 157
    {0x34, 0x44, 0x38, 0xFF}, // 158
    {0x50, 0x3C, 0x04, 0xFF}, // 159
    {0x40, 0x44, 0x08, 0xFF}, // 160
    {0x4C, 0x38, 0x18, 0xFF}, // 161
    {0x50, 0x34, 0x10, 0xFF}, // 162
    {0x54, 0x2C, 0x34, 0xFF}, // 163
    {0x4C, 0x34, 0x18, 0xFF}, // 164
    {0x48, 0x38, 0x14, 0xFF}, // 165
    {0x48, 0x38, 0x04, 0xFF}, // 166
    {0x34, 0x30, 0x5C, 0xFF}, // 167
    {0x48, 0x30, 0x38, 0xFF}, // 168
    {0xAC, 0x00, 0x00, 0xFF}, // 169
    {0x30, 0x3C, 0x34, 0xFF}, // 170
    {0x3C, 0x3C, 0x08, 0xFF}, // 171
    {0x18, 0x48, 0x08, 0xFF}, // 172
    {0x44, 0x30, 0x14, 0xFF}, // 173
    {0x44, 0x30, 0x0C, 0xFF}, // 174
    {0x4C, 0x28, 0x0C, 0xFF}, // 175
    {0x44, 0x28, 0x2C, 0xFF}, // 176
    {0x38, 0x38, 0x04, 0xFF}, // 177
    {0x40, 0x28, 0x28, 0xFF}, // 178
    {0x3C, 0x30, 0x14, 0xFF}, // 179
    {0x28, 0x28, 0x4C, 0xFF}, // 180
    {0x24, 0x34, 0x2C, 0xFF}, // 181
    {0x38, 0x2C, 0x20, 0xFF}, // 182
    {0x38, 0x2C, 0x08, 0xFF}, // 183
    {0x80, 0x00, 0x00, 0xFF}, // 184
    {0x30, 0x30, 0x04, 0xFF}, // 185
    {0x24, 0x24, 0x44, 0xFF}, // 186
    {0x18, 0x38, 0x04, 0xFF}, // 187
    {0x34, 0x24, 0x08, 0xFF}, // 188
    {0x74, 0x00, 0x00, 0xFF}, // 189
    {0x1C, 0x2C, 0x24, 0xFF}, // 190
    {0x38, 0x1C, 0x20, 0xFF}, // 191
    {0x34, 0x20, 0x08, 0xFF}, // 192
    {0x6C, 0x00, 0x00, 0xFF}, // 193
    {0x68, 0x00, 0x00, 0xFF}, // 194
    {0x28, 0x28, 0x04, 0xFF}, // 195
    {0x60, 0x00, 0x00, 0xFF}, // 196
    {0x1C, 0x1C, 0x34, 0xFF}, // 197
    {0x58, 0x00, 0x00, 0xFF}, // 198
    {0x28, 0x20, 0x04, 0xFF}, // 199
    {0x2C, 0x18, 0x14, 0xFF}, // 200
    {0x14, 0x24, 0x18, 0xFF}, // 201
    {0x0C, 0x28, 0x00, 0xFF}, // 202
    {0x28, 0x18, 0x18, 0xFF}, // 203
    {0x50, 0x00, 0x00, 0xFF}, // 204
    {0x1C, 0x20, 0x00, 0xFF}, // 205
    {0x48, 0x00, 0x00, 0xFF}, // 206
    {0x44, 0x00, 0x00, 0xFF}, // 207
    {0x18, 0x1C, 0x10, 0xFF}, // 208
    {0x44, 0x00, 0x00, 0xFF}, // 209
    {0x1C, 0x18, 0x00, 0xFF}, // 210
    {0x04, 0x20, 0x00, 0xFF}, // 211
    {0x40, 0x00, 0x00, 0xFF}, // 212
    {0x14, 0x10, 0x28, 0xFF}, // 213
    {0x20, 0x10, 0x10, 0xFF}, // 214
    {0x38, 0x00, 0x00, 0xFF}, // 215
    {0x38, 0x00, 0x00, 0xFF}, // 216
    {0x18, 0x14, 0x14, 0xFF}, // 217
    {0x30, 0x00, 0x00, 0xFF}, // 218
    {0x08, 0x18, 0x00, 0xFF}, // 219
    {0x14, 0x14, 0x00, 0xFF}, // 220
    {0x0C, 0x0C, 0x1C, 0xFF}, // 221
    {0x28, 0x00, 0x00, 0xFF}, // 222
    {0x08, 0x10, 0x08, 0xFF}, // 223
    {0x10, 0x08, 0x08, 0xFF}, // 224
    {0x0C, 0x0C, 0x00, 0xFF}, // 225
};
uint8_t paletteDataRes007[256][4] = {
    {0x00, 0x00, 0x00, 0xFF}, // 0
    {0xFC, 0xFC, 0xF4, 0xFF}, // 1
    {0xFC, 0xF4, 0xF4, 0xFF}, // 2
    {0xF4, 0xF4, 0xF4, 0xFF}, // 3
    {0xF4, 0xF4, 0xEC, 0xFF}, // 4
    {0xF4, 0xEC, 0xEC, 0xFF}, // 5
    {0xF4, 0xEC, 0xE4, 0xFF}, // 6
    {0xEC, 0xEC, 0xE4, 0xFF}, // 7
    {0xEC, 0xEC, 0xDC, 0xFF}, // 8
    {0xEC, 0xE4, 0xDC, 0xFF}, // 9
    {0xEC, 0xE4, 0xD4, 0xFF}, // 10
    {0xE4, 0xE4, 0xD4, 0xFF}, // 11
    {0xE4, 0xDC, 0xD4, 0xFF}, // 12
    {0xE4, 0xDC, 0xCC, 0xFF}, // 13
    {0xE4, 0xD4, 0xCC, 0xFF}, // 14
    {0xE4, 0xD4, 0xC4, 0xFF}, // 15
    {0xDC, 0xD4, 0xC4, 0xFF}, // 16
    {0xDC, 0xD4, 0xBC, 0xFF}, // 17
    {0xDC, 0xCC, 0xBC, 0xFF}, // 18
    {0xDC, 0xCC, 0xB4, 0xFF}, // 19
    {0xD4, 0xCC, 0xBC, 0xFF}, // 20
    {0xD4, 0xCC, 0xB4, 0xFF}, // 21
    {0xD4, 0xC4, 0xB4, 0xFF}, // 22
    {0xD4, 0xC4, 0xAC, 0xFF}, // 23
    {0xD4, 0xC4, 0xA4, 0xFF}, // 24
    {0xCC, 0xC4, 0xAC, 0xFF}, // 25
    {0xCC, 0xBC, 0xA4, 0xFF}, // 26
    {0xD4, 0xBC, 0x8C, 0xFF}, // 27
    {0xCC, 0xBC, 0x9C, 0xFF}, // 28
    {0xC4, 0xBC, 0x9C, 0xFF}, // 29
    {0xD4, 0xB4, 0x8C, 0xFF}, // 30
    {0xCC, 0xB4, 0x9C, 0xFF}, // 31
    {0xD4, 0xB4, 0x84, 0xFF}, // 32
    {0xCC, 0xB4, 0x94, 0xFF}, // 33
    {0xD4, 0xB4, 0x78, 0xFF}, // 34
    {0xCC, 0xB4, 0x8C, 0xFF}, // 35
    {0xC4, 0xB4, 0x9C, 0xFF}, // 36
    {0xCC, 0xB4, 0x84, 0xFF}, // 37
    {0xC4, 0xB4, 0x94, 0xFF}, // 38
    {0xCC, 0xB4, 0x78, 0xFF}, // 39
    {0xC4, 0xB4, 0x8C, 0xFF}, // 40
    {0xC4, 0xB4, 0x84, 0xFF}, // 41
    {0xC0, 0xB8, 0xA0, 0xFF}, // 42
    {0xC0, 0xB8, 0x98, 0xFF}, // 43
    {0xCC, 0xAC, 0x84, 0xFF}, // 44
    {0xC4, 0xAC, 0x94, 0xFF}, // 45
    {0xCC, 0xAC, 0x78, 0xFF}, // 46
    {0xC4, 0xAC, 0x8C, 0xFF}, // 47
    {0xCC, 0xAC, 0x70, 0xFF}, // 48
    {0xC4, 0xAC, 0x84, 0xFF}, // 49
    {0xC0, 0xB0, 0x98, 0xFF}, // 50
    {0xC4, 0xAC, 0x78, 0xFF}, // 51
    {0xC0, 0xB0, 0x90, 0xFF}, // 52
    {0xC4, 0xAC, 0x70, 0xFF}, // 53
    {0xC0, 0xB0, 0x88, 0xFF}, // 54
    {0xC0, 0xB0, 0x7C, 0xFF}, // 55
    {0xB8, 0xB0, 0x90, 0xFF}, // 56
    {0xC4, 0xA4, 0x78, 0xFF}, // 57
    {0xC0, 0xA8, 0x90, 0xFF}, // 58
    {0xC4, 0xA4, 0x70, 0xFF}, // 59
    {0xC0, 0xA8, 0x88, 0xFF}, // 60
    {0xC4, 0xA4, 0x68, 0xFF}, // 61
    {0xC0, 0xA8, 0x7C, 0xFF}, // 62
    {0xB8, 0xA8, 0x90, 0xFF}, // 63
    {0xC0, 0xA8, 0x74, 0xFF}, // 64
    {0xB8, 0xA8, 0x88, 0xFF}, // 65
    {0xC0, 0xA8, 0x6C, 0xFF}, // 66
    {0xB8, 0xA8, 0x7C, 0xFF}, // 67
    {0xB8, 0xA8, 0x74, 0xFF}, // 68
    {0xB0, 0xA8, 0x88, 0xFF}, // 69
    {0xC0, 0xA0, 0x74, 0xFF}, // 70
    {0xC0, 0xA0, 0x6C, 0xFF}, // 71
    {0xB8, 0xA0, 0x7C, 0xFF}, // 72
    {0xC0, 0xA0, 0x64, 0xFF}, // 73
    {0xB0, 0xA0, 0x90, 0xFF}, // 74
    {0xB8, 0xA0, 0x74, 0xFF}, // 75
    {0xB0, 0xA0, 0x88, 0xFF}, // 76
    {0xB8, 0xA0, 0x6C, 0xFF}, // 77
    {0xB0, 0xA0, 0x7C, 0xFF}, // 78
    {0xB8, 0xA0, 0x64, 0xFF}, // 79
    {0xB0, 0xA0, 0x74, 0xFF}, // 80
    {0xA8, 0xA0, 0x88, 0xFF}, // 81
    {0xB0, 0xA0, 0x6C, 0xFF}, // 82
    {0xB8, 0x98, 0x6C, 0xFF}, // 83
    {0xB8, 0x98, 0x64, 0xFF}, // 84
    {0xB0, 0x98, 0x74, 0xFF}, // 85
    {0xB0, 0x98, 0x6C, 0xFF}, // 86
    {0xA8, 0x98, 0x7C, 0xFF}, // 87
    {0xB0, 0x98, 0x64, 0xFF}, // 88
    {0xA8, 0x98, 0x74, 0xFF}, // 89
    {0xA8, 0x98, 0x6C, 0xFF}, // 90
    {0xA8, 0x98, 0x64, 0xFF}, // 91
    {0xA0, 0x98, 0x7C, 0xFF}, // 92
    {0xB0, 0x90, 0x64, 0xFF}, // 93
    {0xB0, 0x90, 0x5C, 0xFF}, // 94
    {0xA8, 0x90, 0x6C, 0xFF}, // 95
    {0xA8, 0x90, 0x64, 0xFF}, // 96
    {0xA0, 0x90, 0x74, 0xFF}, // 97
    {0xA8, 0x90, 0x5C, 0xFF}, // 98
    {0xA0, 0x90, 0x6C, 0xFF}, // 99
    {0xA8, 0x90, 0x50, 0xFF}, // 100
    {0xA0, 0x90, 0x64, 0xFF}, // 101
    {0xA0, 0x90, 0x5C, 0xFF}, // 102
    {0xA8, 0x88, 0x5C, 0xFF}, // 103
    {0xA8, 0x88, 0x50, 0xFF}, // 104
    {0xA0, 0x88, 0x64, 0xFF}, // 105
    {0x98, 0x88, 0x74, 0xFF}, // 106
    {0xA0, 0x88, 0x5C, 0xFF}, // 107
    {0x98, 0x88, 0x6C, 0xFF}, // 108
    {0xA0, 0x88, 0x54, 0xFF}, // 109
    {0x98, 0x88, 0x64, 0xFF}, // 110
    {0x98, 0x88, 0x5C, 0xFF}, // 111
    {0x90, 0x88, 0x6C, 0xFF}, // 112
    {0x98, 0x88, 0x54, 0xFF}, // 113
    {0x90, 0x88, 0x64, 0xFF}, // 114
    {0xA0, 0x7C, 0x54, 0xFF}, // 115
    {0x98, 0x7C, 0x64, 0xFF}, // 116
    {0x98, 0x7C, 0x5C, 0xFF}, // 117
    {0x98, 0x7C, 0x54, 0xFF}, // 118
    {0x90, 0x7C, 0x64, 0xFF}, // 119
    {0x98, 0x7C, 0x48, 0xFF}, // 120
    {0x90, 0x7C, 0x5C, 0xFF}, // 121
    {0x90, 0x7C, 0x54, 0xFF}, // 122
    {0x90, 0x7C, 0x4C, 0xFF}, // 123
    {0x88, 0x7C, 0x5C, 0xFF}, // 124
    {0x98, 0x74, 0x48, 0xFF}, // 125
    {0x90, 0x74, 0x5C, 0xFF}, // 126
    {0x90, 0x74, 0x54, 0xFF}, // 127
    {0x90, 0x74, 0x4C, 0xFF}, // 128
    {0x88, 0x74, 0x5C, 0xFF}, // 129
    {0x88, 0x74, 0x54, 0xFF}, // 130
    {0x7C, 0x74, 0x64, 0xFF}, // 131
    {0x88, 0x74, 0x4C, 0xFF}, // 132
    {0x7C, 0x74, 0x54, 0xFF}, // 133
    {0x88, 0x6C, 0x54, 0xFF}, // 134
    {0x88, 0x6C, 0x4C, 0xFF}, // 135
    {0x88, 0x6C, 0x40, 0xFF}, // 136
    {0x7C, 0x6C, 0x5C, 0xFF}, // 137
    {0x7C, 0x6C, 0x54, 0xFF}, // 138
    {0x7C, 0x6C, 0x4C, 0xFF}, // 139
    {0x7C, 0x6C, 0x44, 0xFF}, // 140
    {0x74, 0x6C, 0x54, 0xFF}, // 141
    {0x74, 0x6C, 0x4C, 0xFF}, // 142
    {0x7C, 0x64, 0x4C, 0xFF}, // 143
    {0x7C, 0x64, 0x44, 0xFF}, // 144
    {0x74, 0x64, 0x4C, 0xFF}, // 145
    {0x74, 0x64, 0x44, 0xFF}, // 146
    {0x6C, 0x64, 0x54, 0xFF}, // 147
    {0x74, 0x64, 0x3C, 0xFF}, // 148
    {0x6C, 0x64, 0x4C, 0xFF}, // 149
    {0x6C, 0x64, 0x44, 0xFF}, // 150
    {0x74, 0x5C, 0x44, 0xFF}, // 151
    {0x74, 0x5C, 0x3C, 0xFF}, // 152
    {0x6C, 0x5C, 0x4C, 0xFF}, // 153
    {0x6C, 0x5C, 0x44, 0xFF}, // 154
    {0x6C, 0x5C, 0x3C, 0xFF}, // 155
    {0x64, 0x5C, 0x44, 0xFF}, // 156
    {0x64, 0x5C, 0x3C, 0xFF}, // 157
    {0x6C, 0x54, 0x3C, 0xFF}, // 158
    {0x6C, 0x54, 0x30, 0xFF}, // 159
    {0x64, 0x54, 0x44, 0xFF}, // 160
    {0x64, 0x54, 0x3C, 0xFF}, // 161
    {0x5C, 0x54, 0x4C, 0xFF}, // 162
    {0x64, 0x54, 0x34, 0xFF}, // 163
    {0x5C, 0x54, 0x44, 0xFF}, // 164
    {0x5C, 0x54, 0x3C, 0xFF}, // 165
    {0x5C, 0x54, 0x34, 0xFF}, // 166
    {0x64, 0x4C, 0x34, 0xFF}, // 167
    {0x5C, 0x4C, 0x3C, 0xFF}, // 168
    {0x5C, 0x4C, 0x34, 0xFF}, // 169
    {0x54, 0x4C, 0x44, 0xFF}, // 170
    {0x5C, 0x4C, 0x28, 0xFF}, // 171
    {0x54, 0x4C, 0x3C, 0xFF}, // 172
    {0x54, 0x4C, 0x34, 0xFF}, // 173
    {0x54, 0x4C, 0x2C, 0xFF}, // 174
    {0x4C, 0x4C, 0x3C, 0xFF}, // 175
    {0x54, 0x44, 0x34, 0xFF}, // 176
    {0x54, 0x44, 0x2C, 0xFF}, // 177
    {0x4C, 0x44, 0x3C, 0xFF}, // 178
    {0x4C, 0x44, 0x34, 0xFF}, // 179
    {0x4C, 0x44, 0x2C, 0xFF}, // 180
    {0x44, 0x44, 0x3C, 0xFF}, // 181
    {0x44, 0x44, 0x2C, 0xFF}, // 182
    {0x4C, 0x3C, 0x34, 0xFF}, // 183
    {0x4C, 0x3C, 0x2C, 0xFF}, // 184
    {0x4C, 0x3C, 0x20, 0xFF}, // 185
    {0x44, 0x3C, 0x34, 0xFF}, // 186
    {0x44, 0x3C, 0x2C, 0xFF}, // 187
    {0x44, 0x3C, 0x24, 0xFF}, // 188
    {0x40, 0x40, 0x38, 0xFF}, // 189
    {0x40, 0x40, 0x2C, 0xFF}, // 190
    {0x40, 0x40, 0x24, 0xFF}, // 191
    {0x44, 0x34, 0x2C, 0xFF}, // 192
    {0x44, 0x34, 0x24, 0xFF}, // 193
    {0x40, 0x38, 0x2C, 0xFF}, // 194
    {0x40, 0x38, 0x24, 0xFF}, // 195
    {0x40, 0x38, 0x1C, 0xFF}, // 196
    {0x38, 0x38, 0x30, 0xFF}, // 197
    {0x38, 0x38, 0x24, 0xFF}, // 198
    {0x38, 0x38, 0x1C, 0xFF}, // 199
    {0x40, 0x2C, 0x24, 0xFF}, // 200
    {0x40, 0x2C, 0x1C, 0xFF}, // 201
    {0x38, 0x30, 0x30, 0xFF}, // 202
    {0x38, 0x30, 0x24, 0xFF}, // 203
    {0x38, 0x30, 0x1C, 0xFF}, // 204
    {0x30, 0x30, 0x28, 0xFF}, // 205
    {0x30, 0x30, 0x1C, 0xFF}, // 206
    {0x38, 0x24, 0x1C, 0xFF}, // 207
    {0x30, 0x28, 0x28, 0xFF}, // 208
    {0x30, 0x28, 0x1C, 0xFF}, // 209
    {0x30, 0x28, 0x14, 0xFF}, // 210
    {0x28, 0x28, 0x28, 0xFF}, // 211
    {0x28, 0x28, 0x20, 0xFF}, // 212
    {0x28, 0x28, 0x14, 0xFF}, // 213
    {0x28, 0x20, 0x20, 0xFF}, // 214
    {0x28, 0x20, 0x14, 0xFF}, // 215
    {0x20, 0x20, 0x20, 0xFF}, // 216
    {0x20, 0x20, 0x14, 0xFF}, // 217
    {0x20, 0x20, 0x0C, 0xFF}, // 218
    {0x20, 0x14, 0x14, 0xFF}, // 219
    {0x20, 0x14, 0x0C, 0xFF}, // 220
    { 0x18, 0x18, 0x18, 0xFF }, // 221
    { 0x18, 0x18, 0x0C, 0xFF }, // 222
    { 0x18, 0x00, 0x0C, 0xFF }, // 223
    { 0xFC, 0xFC, 0xFC, 0xFF }, // 224
    { 0x10, 0x10, 0x10, 0xFF }, // 225
    { 0x10, 0x10, 0x00, 0xFF }, // 226
    { 0x10, 0x00, 0x00, 0xFF }, // 227
    { 0x00, 0x00, 0x00, 0xFF }, // 228
    { 0x00, 0x00, 0x00, 0xFF }, // 229
    { 0x00, 0x00, 0x00, 0xFF }, // 230
    { 0x00, 0x00, 0x00, 0xFF }, // 231
    { 0x00, 0x00, 0x00, 0xFF }, // 232
    { 0x00, 0x00, 0x00, 0xFF }, // 233
    { 0x00, 0x00, 0x00, 0xFF }, // 234
    { 0x00, 0x00, 0x00, 0xFF }, // 235
    { 0x00, 0x00, 0x00, 0xFF }, // 236
    { 0x00, 0x00, 0x00, 0xFF }, // 237
    { 0x00, 0x00, 0x00, 0xFF }, // 238
    { 0x00, 0x00, 0x00, 0xFF }, // 239
    { 0x00, 0x00, 0x00, 0xFF }, // 240
    { 0x00, 0x00, 0x00, 0xFF }, // 241
    { 0x00, 0x00, 0x00, 0xFF }, // 242
    { 0x00, 0x00, 0x00, 0xFF }, // 243
    { 0x00, 0x00, 0x00, 0xFF }, // 244
    { 0x00, 0x00, 0x00, 0xFF }, // 245
    { 0x00, 0x00, 0x00, 0xFF }, // 246
    { 0x00, 0x00, 0x00, 0xFF }, // 247
    { 0x00, 0x00, 0x00, 0xFF }, // 248
    { 0x00, 0x00, 0x00, 0xFF }, // 249
    { 0x00, 0x00, 0x00, 0xFF }, // 250
    { 0x00, 0x00, 0x00, 0xFF }, // 251
    { 0x00, 0x00, 0x00, 0xFF }, // 252
    { 0x00, 0x00, 0x00, 0xFF }, // 253
    { 0x00, 0x00, 0x00, 0xFF }, // 254
    { 0x00, 0x00, 0xFC, 0xFF }, // 255
};

std::vector<uint8_t> generateSanitariumPalette(uint8_t paletteData[256][4]) {
    std::vector<uint8_t> palette(256 * 3);

    // Copy the palette data into the output vector
    for (int i = 0; i < 256; i++) {
        palette[i * 3] = paletteData[i][0];     // R
        palette[i * 3 + 1] = paletteData[i][1]; // G
        palette[i * 3 + 2] = paletteData[i][2]; // B
    }

    return palette;
}

// Extracts a single frame from the resource to a BMP file
bool extractFrameToBMP(const char* resourceData, uint32_t frameIndex, const std::string& outputFilename, const std::vector<uint8_t>& palette) {
    uint16_t frameCount = static_cast<uint16_t>(
        static_cast<uint8_t>(resourceData[0x18]) |
        (static_cast<uint8_t>(resourceData[0x19]) << 8)
        );

    if (frameIndex >= frameCount)
        return false;

    uint32_t offsetPos = 0x1C + (frameIndex * 4);
    uint32_t frameOffset = static_cast<uint8_t>(resourceData[offsetPos]) |
        (static_cast<uint8_t>(resourceData[offsetPos + 1]) << 8) |
        (static_cast<uint8_t>(resourceData[offsetPos + 2]) << 16) |
        (static_cast<uint8_t>(resourceData[offsetPos + 3]) << 24);

    uint32_t offsetsArrayEnd = 0x1C + (frameCount * 4);
    uint32_t framePosition = offsetsArrayEnd + frameOffset;

    uint16_t height = static_cast<uint16_t>(
        static_cast<uint8_t>(resourceData[framePosition + 0x0C]) |
        (static_cast<uint8_t>(resourceData[framePosition + 0x0C + 1]) << 8)
        );

    uint16_t width = static_cast<uint16_t>(
        static_cast<uint8_t>(resourceData[framePosition + 0x0E]) |
        (static_cast<uint8_t>(resourceData[framePosition + 0x0E + 1]) << 8)
        );

    // Raw pixel data starts at offset 0x10 from frame header
    const uint8_t* indexedData = reinterpret_cast<const uint8_t*>(resourceData + framePosition + 0x10);

    // Here we calculate the size of the BMP
    // Each pixel in BMP uses 3 bytes (RGB), values go from 0 to 255. We need to add the
    // extra 3 bytes to make sure we're rounding up correctly
    int paddedWidth = (width * 3 + 3) & ~3;
    int imageDataSize = paddedWidth * height;

    BMPHeader bmpHeader;
    DIBHeader dibHeader;

    // -- BMP HEADER --
    bmpHeader.signature = 0x4D42; // 'BM'
    bmpHeader.fileSize = sizeof(BMPHeader) + sizeof(DIBHeader) + imageDataSize;
    bmpHeader.reserved1 = 0;
    bmpHeader.reserved2 = 0;
    bmpHeader.dataOffset = sizeof(BMPHeader) + sizeof(DIBHeader);

    // -- DIB HEADER --
    dibHeader.headerSize = sizeof(DIBHeader);
    dibHeader.width = width;
    dibHeader.height = height;
    dibHeader.planes = 1;
    dibHeader.bitsPerPixel = 24; // RGB
    dibHeader.compression = 0; // No compression
    dibHeader.imageSize = 0; // We can actually leave this at 0 if compression = 0
    dibHeader.xPixelsPerM = 0; // Same as previous point
    dibHeader.yPixelsPerM = 0; // 
    dibHeader.colorsUsed = 256;
    dibHeader.importantColors = 0;

    // Output
    std::ofstream file(outputFilename, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    // Creating headers
    file.write(reinterpret_cast<const char*>(&bmpHeader), sizeof(BMPHeader));
    file.write(reinterpret_cast<const char*>(&dibHeader), sizeof(DIBHeader));

    // Allocate buffer for one row of RGB data with padding
    std::vector<uint8_t> rowBuffer(paddedWidth, 0);

    // Write pixel data (bottom-up)
    for (int y = height - 1; y >= 0; --y) {
        for (int x = 0; x < width; ++x) {
            uint8_t index = indexedData[y * width + x];

            // Convert indexed color to RGB using palette
            // Each index points to a set of 3 values in the palette (R,G,B)
            rowBuffer[x * 3] = palette[index * 3 + 2];     // B
            rowBuffer[x * 3 + 1] = palette[index * 3 + 1];     // G
            rowBuffer[x * 3 + 2] = palette[index * 3];     // R
        }

        // Write the row
        file.write(reinterpret_cast<const char*>(rowBuffer.data()), paddedWidth);
    }

    file.close();
    return true;
}

// Extracts the frames to a single spritesheet instead of separate frames
bool extractFramesToSpritesheet(const char* resourceData, const std::string& outputFilename, const std::vector<uint8_t>& palette) {
    uint16_t frameCount = static_cast<uint16_t>(
        static_cast<uint8_t>(resourceData[0x18]) |
        (static_cast<uint8_t>(resourceData[0x19]) << 8)
        );

    if (frameCount == 0)
        return false;

    std::vector<uint16_t> frameWidths(frameCount);
    std::vector<uint16_t> frameHeights(frameCount);
    std::vector<uint32_t> framePositions(frameCount);

    uint32_t offsetsArrayEnd = 0x1C + (frameCount * 4);
    uint32_t totalWidth = 0;
    uint16_t maxHeight = 0;

    for (uint16_t i = 0; i < frameCount; ++i) {
        uint32_t offsetPos = 0x1C + (i * 4);
        uint32_t frameOffset = static_cast<uint8_t>(resourceData[offsetPos]) |
            (static_cast<uint8_t>(resourceData[offsetPos + 1]) << 8) |
            (static_cast<uint8_t>(resourceData[offsetPos + 2]) << 16) |
            (static_cast<uint8_t>(resourceData[offsetPos + 3]) << 24);

        uint32_t framePosition = offsetsArrayEnd + frameOffset;
        framePositions[i] = framePosition;

        uint16_t height = static_cast<uint16_t>(
            static_cast<uint8_t>(resourceData[framePosition + 0x0C]) |
            (static_cast<uint8_t>(resourceData[framePosition + 0x0C + 1]) << 8)
            );

        uint16_t width = static_cast<uint16_t>(
            static_cast<uint8_t>(resourceData[framePosition + 0x0E]) |
            (static_cast<uint8_t>(resourceData[framePosition + 0x0E + 1]) << 8)
            );

        frameWidths[i] = width;
        frameHeights[i] = height;

        totalWidth += width;
        maxHeight = std::max(maxHeight, height);
    }

    uint32_t targetWidth = static_cast<uint32_t>(std::sqrt(totalWidth * maxHeight));

    uint32_t currentX = 0;
    uint32_t currentY = 0;
    uint32_t rowHeight = 0;
    uint32_t spritesheetWidth = 0;
    uint32_t spritesheetHeight = 0;

    std::vector<std::pair<uint32_t, uint32_t>> framePositionsInSheet(frameCount);

    // Positions for each frame in the spritesheet
    for (uint16_t i = 0; i < frameCount; ++i) {
        // If this frame won't fit on current row, move to next row
        if (currentX + frameWidths[i] > targetWidth && currentX > 0) {
            currentX = 0;
            currentY += rowHeight;
            rowHeight = 0;
        }

        framePositionsInSheet[i] = { currentX, currentY };

        // Update position for next frame
        currentX += frameWidths[i];
        rowHeight = std::max(rowHeight, static_cast<uint32_t>(frameHeights[i]));

        // Update spritesheet dimensions
        spritesheetWidth = std::max(spritesheetWidth, currentX);
        spritesheetHeight = std::max(spritesheetHeight, currentY + rowHeight);
    }

    if (spritesheetWidth > 8192 || spritesheetHeight > 8192) {
        std::cerr << "Spritesheet dimensions too large: " << spritesheetWidth << "x" << spritesheetHeight << std::endl;
        return false;
    }

    int paddedWidth = (spritesheetWidth * 3 + 3) & ~3;
    int imageDataSize = paddedWidth * spritesheetHeight;

    BMPHeader bmpHeader;
    DIBHeader dibHeader;

    // -- BMP HEADER --
    bmpHeader.signature = 0x4D42; // 'BM'
    bmpHeader.fileSize = sizeof(BMPHeader) + sizeof(DIBHeader) + imageDataSize;
    bmpHeader.reserved1 = 0;
    bmpHeader.reserved2 = 0;
    bmpHeader.dataOffset = sizeof(BMPHeader) + sizeof(DIBHeader);

    // -- DIB HEADER --
    dibHeader.headerSize = sizeof(DIBHeader);
    dibHeader.width = spritesheetWidth;
    dibHeader.height = spritesheetHeight;
    dibHeader.planes = 1;
    dibHeader.bitsPerPixel = 24; // RGB
    dibHeader.compression = 0; // No compression
    dibHeader.imageSize = 0; // Can leave as 0 if no compression
    dibHeader.xPixelsPerM = 0;
    dibHeader.yPixelsPerM = 0;
    dibHeader.colorsUsed = 256;
    dibHeader.importantColors = 0;

    std::ofstream file(outputFilename, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    file.write(reinterpret_cast<const char*>(&bmpHeader), sizeof(BMPHeader));
    file.write(reinterpret_cast<const char*>(&dibHeader), sizeof(DIBHeader));

    // White pixel data by default
    std::vector<std::vector<uint8_t>> pixelData(spritesheetHeight, std::vector<uint8_t>(spritesheetWidth * 3, 255));

    for (uint16_t i = 0; i < frameCount; ++i) {
        uint32_t frameX = framePositionsInSheet[i].first;
        uint32_t frameY = framePositionsInSheet[i].second;
        uint16_t width = frameWidths[i];
        uint16_t height = frameHeights[i];

        // Raw pixel data starts at offset 0x10 from frame header
        const uint8_t* indexedData = reinterpret_cast<const uint8_t*>(resourceData + framePositions[i] + 0x10);

        // Copy frame data to spritesheet
        for (uint16_t y = 0; y < height; ++y) {
            for (uint16_t x = 0; x < width; ++x) {
                uint8_t index = indexedData[y * width + x];

                // Ensure we don't write outside the buffer
                if (frameY + y < spritesheetHeight && frameX + x < spritesheetWidth) {
                    // In BMP, pixels are ordered as BGR
                    pixelData[frameY + y][(frameX + x) * 3 + 0] = palette[index * 3 + 2]; // B
                    pixelData[frameY + y][(frameX + x) * 3 + 1] = palette[index * 3 + 1]; // G
                    pixelData[frameY + y][(frameX + x) * 3 + 2] = palette[index * 3 + 0]; // R
                }
            }
        }
    }

    std::vector<uint8_t> rowBuffer(paddedWidth, 0);

    // Write pixel data (bottom-up)
    for (int y = spritesheetHeight - 1; y >= 0; --y) {
        // Copy row data to padded buffer
        std::memcpy(rowBuffer.data(), pixelData[y].data(), pixelData[y].size());

        // Write the row
        file.write(reinterpret_cast<const char*>(rowBuffer.data()), paddedWidth);
    }

    file.close();

    std::cout << "Created spritesheet with " << frameCount << " frames, dimensions: "
        << spritesheetWidth << "x" << spritesheetHeight << std::endl;

    return true;
}

// Just to make sure Windows doesn't get mad at me (:
std::string cleanFolderName(const std::string& input) {
    std::string result = input;

    // Remove path information - keep only filename
    size_t lastSlash = result.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        result = result.substr(lastSlash + 1);
    }

    // Remove invalid characters
    result.erase(std::remove_if(result.begin(), result.end(),
        [](char c) {
            return c == '<' || c == '>' || c == ':' ||
                c == '"' || c == '/' || c == '\\' ||
                c == '|' || c == '?' || c == '*' ||
                c < 32;
        }),
        result.end());

    return result;
}

// -- MAIN EXTRACTION FUNCTION --
bool extractFiles(const std::string& filename, FileFormat format, bool extractIndividualFrames = true, bool extractSpritesheet = false, const std::vector<uint8_t>& palette = std::vector<uint8_t>()) {
    const FormatInfo& info = formatInfoMap.at(format);

    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return false;
    }

    // Read file into buffer
    std::vector<char> fileBuffer((std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());

    file.close();

    if (fileBuffer.empty()) {
        std::cerr << "File is empty" << std::endl;
        return false;
    }

    std::cout << "File size: " << fileBuffer.size() << " bytes" << std::endl;
    std::cout << "Searching for " << info.name << " files..." << std::endl;

    size_t position = 0;
    int fileCount = 0;
    int frameCount = 0; // For counting total frames (for D3GR)


    std::string cleanFilename = cleanFolderName(filename);
    std::filesystem::create_directory(info.folderName);
    std::string subfolder = info.folderName + "/" + cleanFilename;
    std::filesystem::create_directory(subfolder);

    // Function pointers for header detection and size calculation
    size_t(*findHeader)(const char*, size_t) = nullptr;
    uint32_t(*getSize)(const char*) = nullptr;

    switch (format) {
    case FileFormat::WAV:
        findHeader = findWavHeader;
        getSize = getWavSize;
        break;
    case FileFormat::D3GR:
        findHeader = findGraphicsResourceHeader;
        getSize = getGraphicsResourceSize;
        break;
    }

    // Keep searching until we reach the end of the file
    while (position < fileBuffer.size()) {
        size_t headerPos = findHeader(&fileBuffer[position], fileBuffer.size() - position);

        if (headerPos == SIZE_MAX) {
            break;
        }

        // Calculate absolute position
        size_t fileStart = position + headerPos;

        size_t minHeaderSize = 16; // Minimum bytes needed to read header and size
        if (fileStart + minHeaderSize > fileBuffer.size()) {
            break;
        }

        // This will happen if our current buffer is too small for the file size.
        // TODO: reload the buffer while keeping the current data to avoid truncating files
        // For most files it shouldn't be an issue
        uint32_t fileSize = getSize(&fileBuffer[fileStart]);
        if (fileStart + fileSize > fileBuffer.size()) {
            std::cout << "Warning: " << info.name << " file appears truncated. Requested size: " << fileSize
                << ", but only " << (fileBuffer.size() - fileStart) << " bytes available." << std::endl;
            fileSize = fileBuffer.size() - fileStart;
        }

        std::cout << "Found " << info.name << " file at position " << fileStart
            << ", size: " << fileSize << " bytes" << std::endl;

        // Create resource raw file
        std::string resourceFileName = subfolder + "/" + info.extension + "_" + std::to_string(fileCount++) + "." + info.extension;
        std::ofstream resourceFile(resourceFileName, std::ios::binary);

        if (!resourceFile) {
            std::cerr << "Failed to create output file: " << resourceFileName << std::endl;
            continue;
        }

        resourceFile.write(&fileBuffer[fileStart], fileSize);
        resourceFile.close();

        std::cout << "Extracted raw resource to " << resourceFileName << std::endl;

        // Special handling for D3GR format
        if (format == FileFormat::D3GR) {
            std::string framesFolder = subfolder + "/frames_" + std::to_string(fileCount - 1);
            std::filesystem::create_directory(framesFolder);

            uint16_t d3grFrameCount = static_cast<uint16_t>(
                static_cast<uint8_t>(fileBuffer[fileStart + 0x18]) |
                (static_cast<uint8_t>(fileBuffer[fileStart + 0x19]) << 8)
                );

            std::cout << "  Resource contains " << d3grFrameCount << " frames" << std::endl;

            // Extract each frame if individual frames are requested
            if (extractIndividualFrames) {
                int extractedFrames = 0;
                for (uint16_t i = 0; i < d3grFrameCount; ++i) {
                    std::string framePath = framesFolder + "/frame_" + std::to_string(i) + ".bmp";

                    if (extractFrameToBMP(&fileBuffer[fileStart], i, framePath, palette)) {
                        extractedFrames++;
                        frameCount++;
                    }
                }

                std::cout << "  Extracted " << extractedFrames << " frames as BMP files to " << framesFolder << std::endl;
            }

            // Extract frames as spritesheet if requested
            if (extractSpritesheet) {
                std::string spritesheetPath = subfolder + "/spritesheet_" + std::to_string(fileCount - 1) + ".bmp";
                if (extractFramesToSpritesheet(&fileBuffer[fileStart], spritesheetPath, palette)) {
                    std::cout << "  Extracted spritesheet to " << spritesheetPath << std::endl;
                }
                else {
                    std::cout << "  Failed to create spritesheet" << std::endl;
                }
            }
        }

        // Move to the end of this file for next search
        position = fileStart + fileSize;
    }

    std::cout << "Extracted " << fileCount << " " << info.name << " files" << std::endl;
    if (format == FileFormat::D3GR && frameCount > 0) {
        std::cout << "Total frames extracted: " << frameCount << std::endl;
    }
    return fileCount > 0;
}

// -- PALETTE DATA --
// TODO: Move this to the top along with all other enums/structs/etc. Keeping this close for now
std::map<std::string, std::vector<uint8_t>> filenameToPalette = {
    {"RES.006", generateSanitariumPalette(paletteDataRes006)},
    {"RES.007", generateSanitariumPalette(paletteDataRes007)},
    {"RES.008", generateSanitariumPalette(paletteDataRes006)},
    {"RES.009", generateSanitariumPalette(paletteDataRes006)}
};


int main() {
    std::string filename = "";
    bool extractIndividualFrames = true;  // Default to true for backward compatibility
    bool extractSpritesheet = false;     // Default to false
    // Defaulting palette value to the one for RES.006
	std::vector<uint8_t> palette = generateSanitariumPalette(paletteDataRes007);

    while (true) {
        std::cout << "Enter the filename to scan (type EXIT to close the program): ";
        std::getline(std::cin, filename);

        if (filename == "EXIT") {
	        std::cout << "Exiting program..." << std::endl;
	        break;
        }
        else {
            auto it = filenameToPalette.find(filename);
            if (it != filenameToPalette.end()) {
                std::vector<uint8_t> palette = it->second;
                std::cout << "Palette for " << filename << " has been set.\n";
            }
            else {
                std::cout << "No palette found for " << filename << ".\n";
            }
        }

        // Display available formats to extract
        std::cout << "\nAvailable formats to extract:" << std::endl;
        int i = 1;
        for (const auto& format : formatInfoMap) {
            std::cout << i++ << ". " << format.second.name << " (." << format.second.extension << ")" << std::endl;
        }

        // Get user choice
        int choice = 0;
        std::string choiceStr;
        while (choice < 1 || choice > formatInfoMap.size()) {
            std::cout << "\nSelect format to extract (1-" << formatInfoMap.size() << "): ";
            std::getline(std::cin, choiceStr);

            try {
                choice = std::stoi(choiceStr);
            }
            catch (...) {
                choice = 0;
            }
        }

        // Convert choice to format enum
        FileFormat selectedFormat = static_cast<FileFormat>(choice - 1);

        // If D3GR format was selected, ask for extraction options
        if (selectedFormat == FileFormat::D3GR) {
            std::cout << "\nD3GR extraction options:" << std::endl;
            std::cout << "1. Extract individual frames" << std::endl;
            std::cout << "2. Extract spritesheet" << std::endl;
            std::cout << "3. Extract both" << std::endl;

            int extractOption = 0;
            while (extractOption < 1 || extractOption > 3) {
                std::cout << "Select option (1-3): ";
                std::getline(std::cin, choiceStr);

                try {
                    extractOption = std::stoi(choiceStr);
                }
                catch (...) {
                    extractOption = 0;
                }
            }

            // Set extraction flags based on user choice
            switch (extractOption) {
            case 1:
                extractIndividualFrames = true;
                extractSpritesheet = false;
                break;
            case 2:
                extractIndividualFrames = false;
                extractSpritesheet = true;
                break;
            case 3:
                extractIndividualFrames = true;
                extractSpritesheet = true;
                break;
            }
        }

        // Extract files of the chosen format
        bool success = extractFiles(filename, selectedFormat, extractIndividualFrames, extractSpritesheet, palette);

        if (success) {
            std::cout << "Extraction completed successfully!" << std::endl;
        }
        else {
            std::cout << "No files were extracted." << std::endl;
        }

        // Add a visual separator between extraction sessions
        std::cout << "\n----------------------------------------\n" << std::endl;
    }
    return 0;
}