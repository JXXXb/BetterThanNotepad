@echo off
REM Build helper - auto-detects Visual Studio (MSVC) so it also works from a plain terminal
setlocal
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
	echo vswhere.exe not found. Install Visual Studio with the C++ workload.
	exit /b 1
)
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
if not defined VSPATH (
	echo MSVC toolchain not found. Install Visual Studio with the C++ workload.
	exit /b 1
)
call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat"
if %errorlevel% neq 0 (
	echo vcvars64.bat failed.
	exit /b %errorlevel%
)

echo Building RepoManager (all targets: main.exe and logger.exe)...
if not exist build mkdir build
REM Clear a stale cache so CMake re-detects the compiler (prevents keeping MinGW GCC)
if exist build\CMakeCache.txt (
	echo Clearing previous CMake cache to re-detect the compiler...
	del /q build\CMakeCache.txt
	if exist build\CMakeFiles rmdir /s /q build\CMakeFiles
)
echo Configuring with CMake (generator: Ninja, compiler: MSVC)...
cmake -S . -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release
if %errorlevel% neq 0 (
	echo CMake configuration failed.
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
