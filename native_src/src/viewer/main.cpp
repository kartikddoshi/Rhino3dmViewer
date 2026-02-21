#include <windows.h>
#include <commdlg.h>
#include <commctrl.h>
#include <shlobj.h>
#include <gdiplus.h>
#include <string>
#include <vector>
#include <filesystem>
#include <thread>
#include <atomic>
#include <mutex>
#include <map>
#include "../core/ThumbnailExtractor.h"
#include "opennurbs.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shlwapi.lib")

// Global variables
ThumbnailData g_singleThumbnail = {0, 0, 0, {}, false};
HWND g_hTreeView;
HWND g_hListView;
HWND g_hMainWindow;
HIMAGELIST g_hImageListThumbnails;
HFONT g_hFont;
std::atomic<bool> g_loadingThumbnails(false);
std::thread g_loadingThread;
ULONG_PTR g_gdiplusToken;
bool g_isPopulating = false;

// Map to store full paths for TreeView items
std::map<HTREEITEM, std::wstring> g_treePaths;

// Control IDs
#define IDC_TREEVIEW 101
#define IDC_LISTVIEW 102
#define WM_ADD_DRIVE (WM_USER + 2)

// Menu IDs
#define IDM_COPY 201
#define IDM_RENAME 202
#define IDM_OPENEXPLORER 203

struct DriveInfo {
    wchar_t path[4];
    wchar_t displayName[MAX_PATH];
    int iconIndex;
};

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

// Helper to copy file to clipboard
void CopyFileToClipboard(HWND hwnd, const std::wstring& path) {
    if (OpenClipboard(hwnd)) {
        EmptyClipboard();
        
        size_t size = sizeof(DROPFILES) + (path.length() + 2) * sizeof(wchar_t);
        HGLOBAL hGlobal = GlobalAlloc(GHND, size);
        if (hGlobal) {
            DROPFILES* df = (DROPFILES*)GlobalLock(hGlobal);
            df->pFiles = sizeof(DROPFILES);
            df->fWide = TRUE;
            wcscpy_s((wchar_t*)(df + 1), path.length() + 1, path.c_str());
            GlobalUnlock(hGlobal);
            
            SetClipboardData(CF_HDROP, hGlobal);
        }
        CloseClipboard();
    }
}

// Helper to open file in Explorer
void OpenInExplorer(const std::wstring& path) {
    std::wstring param = L"/select,\"" + path + L"\"";
    ShellExecuteW(NULL, L"open", L"explorer.exe", param.c_str(), NULL, SW_SHOW);
}

void SetStandardFont(HWND hwnd) {
    if (!g_hFont) {
        NONCLIENTMETRICS ncm = { sizeof(NONCLIENTMETRICS) };
        SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &ncm, 0);
        g_hFont = CreateFontIndirect(&ncm.lfMessageFont);
    }
    SendMessage(hwnd, WM_SETFONT, (WPARAM)g_hFont, TRUE);
}

HIMAGELIST GetSystemImageList(bool smallIcon) {
    SHFILEINFOW sfi = {0};
    UINT flags = SHGFI_SYSICONINDEX | (smallIcon ? SHGFI_SMALLICON : SHGFI_LARGEICON);
    return (HIMAGELIST)SHGetFileInfoW(L"C:\\", 0, &sfi, sizeof(SHFILEINFOW), flags);
}

int GetIconIndex(const std::wstring& path) {
    SHFILEINFOW sfi = {0};
    UINT flags = SHGFI_SYSICONINDEX | SHGFI_SMALLICON;
    DWORD attrs = 0;

    if (path.length() > 3) {
        flags |= SHGFI_USEFILEATTRIBUTES;
        attrs = FILE_ATTRIBUTE_DIRECTORY;
    }

    SHGetFileInfoW(path.c_str(), attrs, &sfi, sizeof(SHFILEINFOW), flags);
    return sfi.iIcon;
}

HBITMAP CreateHBitmapFromThumbnail(const ThumbnailData& data, int targetSize) {
    if (!data.isValid || data.pixels.empty()) return NULL;
    
    int stride = ((((data.width * data.bitsPerPixel) + 31) & ~31) >> 3);
    Gdiplus::Bitmap* bmp = new Gdiplus::Bitmap(data.width, data.height, stride, PixelFormat24bppRGB, (BYTE*)data.pixels.data());
    if (bmp->GetLastStatus() != Gdiplus::Ok) {
        delete bmp;
        return NULL;
    }
    
    bmp->RotateFlip(Gdiplus::RotateNoneFlipY);

    Gdiplus::Bitmap* resizedBmp = new Gdiplus::Bitmap(targetSize, targetSize, PixelFormat24bppRGB);
    Gdiplus::Graphics g(resizedBmp);
    g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    g.Clear(Gdiplus::Color::White);
    
    float ratio = (float)data.width / (float)data.height;
    int newW = targetSize;
    int newH = targetSize;
    int x = 0;
    int y = 0;
    
    if (ratio > 1.0f) {
        newH = (int)(targetSize / ratio);
        y = (targetSize - newH) / 2;
    } else {
        newW = (int)(targetSize * ratio);
        x = (targetSize - newW) / 2;
    }
    
    g.DrawImage(bmp, x, y, newW, newH);
    
    HBITMAP hBitmap = NULL;
    resizedBmp->GetHBITMAP(Gdiplus::Color::White, &hBitmap);
    
    delete bmp;
    delete resizedBmp;
    
    return hBitmap;
}

struct LoadedItem {
    std::wstring path;
    ThumbnailData data;
    std::wstring filename;
    std::wstring date;
    std::wstring size;
};

void WorkerThread(std::wstring directory, HWND hwnd) {
    namespace fs = std::filesystem;
    
    if (!fs::exists(directory) || !fs::is_directory(directory)) {
        g_loadingThumbnails = false;
        return;
    }

    try {
        for (const auto& entry : fs::directory_iterator(directory)) {
            if (!g_loadingThumbnails) break;
            
            if (entry.is_regular_file() && entry.path().extension() == L".3dm") {
                LoadedItem* item = new LoadedItem();
                item->path = entry.path().wstring();
                item->filename = entry.path().filename().wstring();
                
                uintmax_t fsize = entry.file_size();
                wchar_t sizeBuf[32];
                if (fsize < 1024 * 1024) {
                    swprintf_s(sizeBuf, L"%.2f KB", (double)fsize / 1024.0);
                } else {
                    swprintf_s(sizeBuf, L"%.2f MB", (double)fsize / (1024.0 * 1024.0));
                }
                item->size = sizeBuf;
                
                auto ftime = entry.last_write_time();
                time_t cftime = std::chrono::system_clock::to_time_t(
                    std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                        ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now()
                    )
                );
                struct tm timeinfo;
                localtime_s(&timeinfo, &cftime);
                wchar_t dateBuf[64];
                wcsftime(dateBuf, 64, L"%y%m%d %H:%M", &timeinfo);
                item->date = dateBuf;

                item->data = ThumbnailExtractor::ExtractThumbnail(item->path.c_str());
                
                PostMessage(hwnd, WM_USER + 1, (WPARAM)item, 0);
                
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    } catch (...) {}
    g_loadingThumbnails = false;
}

void LoadFolder(std::wstring folderPath) {
    ListView_DeleteAllItems(g_hListView);
    ImageList_RemoveAll(g_hImageListThumbnails);
    
    g_loadingThumbnails = false;
    if (g_loadingThread.joinable()) g_loadingThread.join();
    
    g_loadingThumbnails = true;
    g_loadingThread = std::thread(WorkerThread, folderPath, g_hMainWindow);
}

void AddTreeItem(HTREEITEM hParent, const std::wstring& path, const std::wstring& displayName, int iconIndex, bool hasChildren) {
    TVINSERTSTRUCTW tvis = {0};
    tvis.hParent = hParent;
    tvis.hInsertAfter = TVI_SORT;
    tvis.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_CHILDREN;
    tvis.item.pszText = (LPWSTR)displayName.c_str();
    tvis.item.iImage = iconIndex;
    tvis.item.iSelectedImage = iconIndex;
    tvis.item.cChildren = hasChildren ? 1 : 0;
    
    HTREEITEM hItem = TreeView_InsertItem(g_hTreeView, &tvis);
    g_treePaths[hItem] = path;
}

void AsyncPopulateDrives(HWND hwnd) {
    DWORD drives = GetLogicalDrives();
    for (int i = 0; i < 26; i++) {
        if (drives & (1 << i)) {
            DriveInfo* info = new DriveInfo();
            info->path[0] = L'A' + i;
            info->path[1] = L':';
            info->path[2] = L'\\';
            info->path[3] = L'\0';
            
            std::wstring name = std::wstring(L"Local Disk (") + info->path + L")";
            wcscpy_s(info->displayName, name.c_str());
            
            info->iconIndex = GetIconIndex(info->path);
            
            PostMessage(hwnd, WM_ADD_DRIVE, (WPARAM)info, 0);
        }
    }
}

void ExpandTreeItem(HTREEITEM hItem) {
    if (TreeView_GetChild(g_hTreeView, hItem) != NULL) return;
    
    std::wstring path = g_treePaths[hItem];
    if (path.empty()) return;
    
    namespace fs = std::filesystem;
    try {
        for (const auto& entry : fs::directory_iterator(path)) {
            if (entry.is_directory()) {
                DWORD attrs = GetFileAttributesW(entry.path().c_str());
                if (attrs & FILE_ATTRIBUTE_HIDDEN) continue;
                
                std::wstring subPath = entry.path().wstring();
                std::wstring name = entry.path().filename().wstring();
                
                int iconIndex = GetIconIndex(subPath);

                bool hasSubDirs = false;
                try {
                    for (const auto& sub : fs::directory_iterator(subPath)) {
                        if (sub.is_directory()) {
                            hasSubDirs = true;
                            break;
                        }
                    }
                } catch (...) {}
                
                AddTreeItem(hItem, subPath, name, iconIndex, hasSubDirs);
            }
        }
    } catch (...) {}
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, NULL);

    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_LISTVIEW_CLASSES | ICC_TREEVIEW_CLASSES;
    InitCommonControlsEx(&icex);
    
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    ON::Begin();

    const wchar_t CLASS_NAME[] = L"RhinoThumbsViewer";

    WNDCLASSW wc = { };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(
        0, CLASS_NAME, L"Rhino 3dm Thumbnail Viewer",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 1200, 800,
        NULL, NULL, hInstance, NULL
    );

    if (hwnd == NULL) return 0;
    g_hMainWindow = hwnd;

    g_hTreeView = CreateWindowExW(0, WC_TREEVIEWW, L"",
        WS_VISIBLE | WS_CHILD | WS_BORDER | TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT | TVS_SHOWSELALWAYS,
        0, 0, 300, 800, hwnd, (HMENU)IDC_TREEVIEW, hInstance, NULL);
    SetStandardFont(g_hTreeView);
    
    HIMAGELIST hSysImgList = GetSystemImageList(true);
    TreeView_SetImageList(g_hTreeView, hSysImgList, TVSIL_NORMAL);

    g_hListView = CreateWindowExW(0, WC_LISTVIEWW, L"",
        WS_VISIBLE | WS_CHILD | WS_BORDER | LVS_ICON | LVS_AUTOARRANGE | LVS_SHOWSELALWAYS | LVS_EDITLABELS,
        300, 0, 900, 800, hwnd, (HMENU)IDC_LISTVIEW, hInstance, NULL);
    SetStandardFont(g_hListView);
        
    g_hImageListThumbnails = ImageList_Create(256, 256, ILC_COLOR24, 10, 10);
    ListView_SetImageList(g_hListView, g_hImageListThumbnails, LVSIL_NORMAL);
    ListView_SetIconSpacing(g_hListView, 320, 340);

    ShowWindow(hwnd, nCmdShow);
    
    std::thread(AsyncPopulateDrives, hwnd).detach();

    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    g_loadingThumbnails = false;
    if (g_loadingThread.joinable()) g_loadingThread.join();
    
    ON::End();
    Gdiplus::GdiplusShutdown(g_gdiplusToken);
    CoUninitialize();

    return 0;
}

// Helper to get full path of selected item
std::wstring GetSelectedFilePath() {
    int iItem = ListView_GetNextItem(g_hListView, -1, LVNI_SELECTED);
    if (iItem == -1) return L"";
    
    HTREEITEM hItem = TreeView_GetSelection(g_hTreeView);
    if (!hItem) return L"";
    std::wstring folderPath = g_treePaths[hItem];
    
    wchar_t buf[MAX_PATH];
    ListView_GetItemText(g_hListView, iItem, 0, buf, MAX_PATH);
    
    std::wstring text = buf;
    size_t pos = text.find(L'\n');
    if (pos != std::wstring::npos) {
        text = text.substr(0, pos);
    }
    
    if (folderPath.back() != L'\\') folderPath += L"\\";
    return folderPath + text;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_NOTIFY:
        {
            LPNMHDR lpnmh = (LPNMHDR)lParam;
            if (lpnmh->idFrom == IDC_TREEVIEW) {
                if (lpnmh->code == TVN_SELCHANGED) {
                    if (g_isPopulating) return 0;
                    LPNMTREEVIEW lpnmtv = (LPNMTREEVIEW)lParam;
                    std::wstring path = g_treePaths[lpnmtv->itemNew.hItem];
                    if (!path.empty()) LoadFolder(path);
                }
                else if (lpnmh->code == TVN_ITEMEXPANDING) {
                    LPNMTREEVIEW lpnmtv = (LPNMTREEVIEW)lParam;
                    if (lpnmtv->action == TVE_EXPAND) ExpandTreeItem(lpnmtv->itemNew.hItem);
                }
            }
            else if (lpnmh->idFrom == IDC_LISTVIEW) {
                if (lpnmh->code == NM_RCLICK) {
                    LPNMITEMACTIVATE lpnmitem = (LPNMITEMACTIVATE)lParam;
                    if (lpnmitem->iItem != -1) {
                        HMENU hMenu = CreatePopupMenu();
                        AppendMenuW(hMenu, MF_STRING, IDM_COPY, L"Copy");
                        AppendMenuW(hMenu, MF_STRING, IDM_RENAME, L"Rename");
                        AppendMenuW(hMenu, MF_STRING, IDM_OPENEXPLORER, L"Open in Explorer");
                        
                        POINT pt;
                        GetCursorPos(&pt);
                        TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
                        DestroyMenu(hMenu);
                    }
                }
                else if (lpnmh->code == LVN_ENDLABELEDIT) {
                    NMLVDISPINFO* pdi = (NMLVDISPINFO*)lParam;
                    if (pdi->item.pszText) {
                        std::wstring oldPath = GetSelectedFilePath();
                        if (!oldPath.empty()) {
                            std::filesystem::path p(oldPath);
                            std::filesystem::path newP = p.parent_path() / pdi->item.pszText;
                            
                            try {
                                std::filesystem::rename(p, newP);
                                return TRUE;
                            } catch (...) {
                                MessageBoxW(hwnd, L"Failed to rename file.", L"Error", MB_ICONERROR);
                                return FALSE;
                            }
                        }
                    }
                    return FALSE;
                }
            }
        }
        return 0;

    case WM_COMMAND:
        {
            int id = LOWORD(wParam);
            switch (id) {
            case IDM_COPY:
                {
                    std::wstring path = GetSelectedFilePath();
                    if (!path.empty()) CopyFileToClipboard(hwnd, path);
                }
                break;
            case IDM_RENAME:
                {
                    int iItem = ListView_GetNextItem(g_hListView, -1, LVNI_SELECTED);
                    if (iItem != -1) {
                        ListView_EditLabel(g_hListView, iItem);
                    }
                }
                break;
            case IDM_OPENEXPLORER:
                {
                    std::wstring path = GetSelectedFilePath();
                    if (!path.empty()) OpenInExplorer(path);
                }
                break;
            }
        }
        return 0;

    case WM_ADD_DRIVE:
        {
            DriveInfo* info = (DriveInfo*)wParam;
            if (info) {
                g_isPopulating = true;
                AddTreeItem(TVI_ROOT, info->path, info->displayName, info->iconIndex, true);
                g_isPopulating = false;
                delete info;
            }
        }
        return 0;

    case WM_USER + 1:
        {
            LoadedItem* item = (LoadedItem*)wParam;
            if (item) {
                HBITMAP hBmp = CreateHBitmapFromThumbnail(item->data, 256);
                int imgIndex = -1;
                if (hBmp) {
                    imgIndex = ImageList_Add(g_hImageListThumbnails, hBmp, NULL);
                    DeleteObject(hBmp);
                }
                LVITEMW lvItem;
                lvItem.mask = LVIF_TEXT | LVIF_IMAGE;
                lvItem.iItem = ListView_GetItemCount(g_hListView);
                lvItem.iSubItem = 0;
                std::wstring text = item->filename + L"\n" + item->date + L"   " + item->size;
                lvItem.pszText = (LPWSTR)text.c_str();
                lvItem.iImage = imgIndex;
                ListView_InsertItem(g_hListView, &lvItem);
                delete item;
            }
        }
        return 0;

    case WM_SIZE:
        {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            int treeWidth = 300;
            MoveWindow(g_hTreeView, 0, 0, treeWidth, height, TRUE);
            MoveWindow(g_hListView, treeWidth, 0, width - treeWidth, height, TRUE);
            ListView_SetIconSpacing(g_hListView, 320, 340);
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
