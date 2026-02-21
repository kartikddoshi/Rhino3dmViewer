#pragma once

#include <string>
#include <vector>
#include <cstdint>

// Forward declare OpenNURBS types if possible, or include headers
// For now, we'll include the main header in the cpp to keep this clean, 
// but we might need some types here.
// Actually, OpenNURBS headers are needed for ON_WindowsBitmap if we return that.
// But let's try to return raw data or a simplified struct to decouple.

struct ThumbnailData {
    int width;
    int height;
    int bitsPerPixel;
    std::vector<uint8_t> pixels; // Raw pixel data (usually BGRA or BGR)
    bool isValid;
    std::wstring errorMessage;
};

class ThumbnailExtractor {
public:
    static ThumbnailData ExtractThumbnail(const std::wstring& filePath);
};
