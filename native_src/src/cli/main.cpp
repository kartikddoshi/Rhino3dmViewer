#include <iostream>
#include <fstream>
#include <vector>
#include <windows.h>
#include "../core/ThumbnailExtractor.h"

void SaveBitmap(const char* filename, int width, int height, int bpp, const std::vector<uint8_t>& pixels) {
    std::ofstream file(filename, std::ios::binary);
    if (!file) return;

    // BMP Header
    BITMAPFILEHEADER bfh = {0};
    bfh.bfType = 0x4D42; // "BM"
    bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    bfh.bfSize = bfh.bfOffBits + pixels.size();

    BITMAPINFOHEADER bih = {0};
    bih.biSize = sizeof(BITMAPINFOHEADER);
    bih.biWidth = width;
    bih.biHeight = height; // Top-down if negative, but usually bottom-up
    bih.biPlanes = 1;
    bih.biBitCount = bpp;
    bih.biCompression = BI_RGB;
    bih.biSizeImage = (DWORD)pixels.size();

    file.write((char*)&bfh, sizeof(bfh));
    file.write((char*)&bih, sizeof(bih));
    file.write((char*)pixels.data(), pixels.size());
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <input_3dm> <output_bmp>" << std::endl;
        return 1;
    }

    std::string inputPathStr = argv[1];
    std::string outputPathStr = argv[2];
    
    // Convert to wstring
    std::wstring inputPath(inputPathStr.begin(), inputPathStr.end());

    // Initialize OpenNURBS (needed for static link)
    // ON::Begin(); // Called in ExtractThumbnail or handled by static init?
    // The original code called ON::Begin() in main.cpp. We should do it too.
    
    ThumbnailData td = ThumbnailExtractor::ExtractThumbnail(inputPath);

    if (td.isValid) {
        SaveBitmap(outputPathStr.c_str(), td.width, td.height, td.bitsPerPixel, td.pixels);
        return 0;
    } else {
        std::cerr << "Failed to extract thumbnail: ";
        std::wcerr << td.errorMessage << std::endl;
        return 1;
    }
}
