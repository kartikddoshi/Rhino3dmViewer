#include <windows.h> // Ensure Windows headers are included first
#ifndef ON_CPLUSPLUS
#define ON_CPLUSPLUS
#endif
#include "opennurbs.h"
#include "ThumbnailExtractor.h"

ThumbnailData ThumbnailExtractor::ExtractThumbnail(const std::wstring& filePath) {
    ThumbnailData data = {0, 0, 0, {}, false};

    // Initialize OpenNURBS (if not already done globally, but it's safe to call multiple times if handled right, 
    // though usually ON::Begin() is once per app. We might need a global init in the viewer.)
    // For this utility, let's assume the caller or a static block handles it, or we do it here.
    // ON::Begin(); // This is usually needed.

    ON_wString filename = filePath.c_str();
    
    // Open file
    FILE* fp = ON::OpenFile(filename, L"rb");
    if (!fp) {
        data.errorMessage = L"Failed to open file.";
        return data;
    }

    // DEBUG: Check file header
    char header[32] = {0};
    if (fread(header, 1, 32, fp) < 32) {
        data.errorMessage = L"File is too small (< 32 bytes).";
        ON::CloseFile(fp);
        return data;
    }
    
    // Rewind
    fseek(fp, 0, SEEK_SET);

    // Check for "3D Geometry" signature
    if (strncmp(header, "3D Geometry", 11) != 0) {
        std::wstring headerStr;
        for(int i=0; i<16; i++) {
            wchar_t buf[4];
            swprintf_s(buf, L"%02X ", (unsigned char)header[i]);
            headerStr += buf;
        }
        data.errorMessage = L"Invalid 3dm file signature. Header: " + headerStr;
        ON::CloseFile(fp);
        return data;
    }

    ON_BinaryFile archive(ON::archive_mode::read3dm, fp);

    // Read start section
    int version = 0;
    ON_String comments;
    if (!archive.Read3dmStartSection(&version, comments)) {
        data.errorMessage = L"Failed to read 3dm start section.";
        ON::CloseFile(fp);
        return data;
    }

    // Read properties to find the preview image
    ON_3dmProperties properties;
    if (archive.Read3dmProperties(properties)) {
        if (properties.m_PreviewImage.IsValid()) {
            const ON_WindowsBitmap& bitmap = properties.m_PreviewImage;
            
            data.width = bitmap.Width();
            data.height = bitmap.Height();
            data.bitsPerPixel = bitmap.BitsPerPixel();
            
            size_t imageSize = bitmap.SizeofImage();

            if (bitmap.m_bits) {
                data.pixels.assign(bitmap.m_bits, bitmap.m_bits + imageSize);
                data.isValid = true;
            } else {
                data.errorMessage = L"Bitmap bits are null.";
            }
        } else {
            data.errorMessage = L"Preview image is invalid.";
        }
    } else {
        data.errorMessage = L"Failed to read 3dm properties.";
    }

    ON::CloseFile(fp);
    return data;
}

extern "C" __declspec(dllexport) void FreeThumbnailData(unsigned char* data) {
    if (data) delete[] data;
}

extern "C" __declspec(dllexport) bool GetThumbnail(const wchar_t* path, int* width, int* height, int* bpp, unsigned char** data, int* size) {
    if (!path) return false;
    std::wstring wpath(path);
    ThumbnailData td = ThumbnailExtractor::ExtractThumbnail(wpath);
    if (!td.isValid) return false;

    *width = td.width;
    *height = td.height;
    *bpp = td.bitsPerPixel;
    *size = (int)td.pixels.size();
    
    *data = new unsigned char[*size];
    memcpy(*data, td.pixels.data(), *size);
    
    return true;
}
