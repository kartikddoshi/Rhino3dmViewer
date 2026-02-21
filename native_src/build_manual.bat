@echo off
echo Attempting to build RhinoThumbs...
echo Please run this script from a "Developer Command Prompt for VS 2022" if CMake is not in your global PATH.

cmake -B build -G "Visual Studio 17 2022" -A x64
if %errorlevel% neq 0 (
    echo CMake configure failed.
    echo Please ensure CMake is installed and in your PATH.
    pause
    exit /b %errorlevel%
)

cmake --build build --config Release
if %errorlevel% neq 0 (
    echo Build failed.
    pause
    exit /b %errorlevel%
)

echo Build successful!
pause
