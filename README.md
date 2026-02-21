# RhinoThumbs.NET & Interactive Viewer

A modern, high-performance WPF application for browsing, managing, and interactively viewing Rhino 3D (`.3dm`) files. It extracts embedded preview images instantly and offers a full 3D hardware-accelerated viewport for diving deeper into your CAD models.

## ✨ Features

### Comprehensive File Navigation
*   **Split-View UI**: Built with WPF and CommunityToolkit.Mvvm, featuring a responsive dark-mode layout.
*   **Network Drive Support**: Seamlessly browses local drives as well as connected AND disconnected Mapped Network Drives (attempts automatic reconnection on expansion).
*   **Recursive Background Search**: An instant search bar that filters the current folder or recursively traverses subdirectories in a background thread to find matching `.3dm` files without freezing the app.

### 3D Capabilities
*   **Native Thumbnail Extraction**: Uses a bundled, standalone C++ CLI tool (built with OpenNURBS) to instantly decompress and extract embedded `ON_WindowsBitmap` previews from `.3dm` files.
*   **Interactive 3D Viewer**: Double-click any `.3dm` thumbnail to open a fully hardware-accelerated 3D window (powered by `HelixToolkit.Wpf` and `Rhino3dm`).
    *   **Controls**: Supports Orbit (Right-Click Drag), Pan (Shift+Right-Click Drag), and Zoom (Scroll).
    *   **Viewports**: Dedicated toolbar buttons to snap to Orthographic CAD views (`Top`, `Front`, `Right`) or `Perspective`.
    *   **Measurement Tool**: Advanced 3D raycasting allows you to click any two points directly on the 3D geometry to calculate precise mathematical distances in millimeters.

### Contextual File Operations
*   **Native Copying**: Right-click to copy files into your clipboard (can be pasted directly into Windows Explorer).
*   **Quick Open**: Right-click to "Open in Explorer" to jump straight to the directory of a file or folder.
*   **7-Zip Compression**: Right-click to compress any `.3dm` file or folder into a `.7z` archive using standard background `7z.exe` integration.
*   **Clean Metadata**: Displays file name, date modified, and file size cleanly below every thumbnail.

---

## 🛠 Prerequisites

*   **Windows 10 or 11** (x64)
*   **.NET 10.0 SDK** (for building the UI)
*   **Visual Studio 2022** (with "Desktop development with C++" workload for building the native extractor)
*   **CMake** (3.20 or higher)

## 📁 Project Structure

*   `RhinoThumbs.NET.csproj`: Main WPF application project (C#).
    *   `/ViewModels` & `/Views`: MVVM frontend logic.
    *   `/Services`: Background extraction and `Rhino3dm` native meshing logic.
*   `native_src/`: Source code for the `ThumbnailExtractorCLI` tool (C++).
    *   This is a standalone CMake project that links against OpenNURBS.
    *   *Note: OpenNURBS SDK source code must be located in `native_src/3rdparty/opennurbs` to build from scratch.*

---

## 🚀 Build Instructions

### 1. Building the Native Extractor (C++)
If you need to recompile the native `ThumbnailExtractorCLI.exe`:
```powershell
cd native_src
cmake -B build
cmake --build build --config Release
```
*Copy the resulting executable into the `Tools/` directory of the main .NET project.*

### 2. Building the WPF Application (.NET)
1. Open `RhinoThumbs.NET.sln` in Visual Studio 2022 or use the .NET CLI.
2. Run `dotnet build`. The process will automatically copy the `Tools/ThumbnailExtractorCLI.exe` alongside your compiled assembly.
3. Run `dotnet run` to launch the application.

### 3. Publishing a Standalone Release
To create a complete, self-contained single-file `.exe` that bundles the .NET runtime and external C++ dependencies:
```powershell
dotnet publish -c Release -r win-x64 --self-contained true -p:PublishSingleFile=true -p:IncludeNativeLibrariesForSelfExtract=true
```
The output can be safely zipped and distributed to non-developers.

---

## 🔧 Troubleshooting

*   **No Thumbnails**: 
    *   Ensure `ThumbnailExtractorCLI.exe` is physically present in the same directory as the main executable.
    *   Ensure the `.3dm` files were saved in Rhino with "Save Small" **unchecked** (so a preview image exists).
*   **Missing 7-Zip option**:
    *   Install 7-Zip from their official site if you wish to use the right-click compression feature.
*   **3D Viewer is Blank**:
    *   Some very large or complex NURBS objects without pre-computed render meshes might fail to display. The application attempts to use existing render meshes to maintain speed.
