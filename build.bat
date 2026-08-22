@echo off
REM Build helper - requires Developer Command Prompt or vcvars64.bat in PATH
echo Building RepoManager (all targets: main.exe and logger.exe)...
if not exist build mkdir build
echo Configuring with CMake (generator: Ninja)...
cmake -S . -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release
if %errorlevel% neq 0 (
	echo CMake configuration failed. Ensure you have a CMake generator available and you ran this from a Developer Command Prompt if using MSVC.
	exit /b %errorlevel%
)
cmake --build build
if %errorlevel% neq 0 (
	echo Build failed.
	exit /b %errorlevel%
)
echo Build finished. Binaries are under build\\ (or in the Visual Studio configuration folder).
echo To run: main.exe
pause
