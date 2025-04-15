#ifndef FILE_FORMATS_H
#define FILE_FORMATS_H

#include <string>
#include <vector>
#include <regex>

/**
 * @struct FileFormat
 * @brief Represents metadata about a file format
 */
struct FileFormat {
    std::string format;        // Format identifier (e.g., "WAV")
    std::string description;   // Human-readable description
    std::string extension;     // File extension including dot
    std::string header_bytes;  // Hexadecimal representation of header bytes
};

/**
 * @var kFileFormats
 * @brief Collection of known file formats with their detection information
 */
const std::vector<FileFormat> kFileFormats = {
    // WAV Format
    {
        "WAV",
        "RIFF WAVE Audio File",
        ".wav",
        "52 49 46 46 xx xx xx xx 57 41 56 45 66 6D 74 20"
    },

    // JPEG-2000 Format
    {
        "JP2",
        "JPEG-2000 Image File",
        ".jp2",
        "00 00 00 0C 6A 50 20 20 0D 0A"
    },

    // BMP Format
    {
        "BMP",
        "Bitmap Image File",
        ".bmp",
        "42 4D"
    }
};

#endif // FILE_FORMATS_H