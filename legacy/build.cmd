@echo off
rem Headless build of the legacy-line DLL. Output: legacy\build\NPCAIAutoPatch.dll
rem Uses the CommonLibSSE (Colorglass line), spdlog and fmt that the Campfire 2026 legacy build already installed through vcpkg,
rem so no network is needed. To build from scratch instead, give legacy a vcpkg manifest like Campfire-2026\plugin\build-local.
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>nul
set PKGS=E:/WorkSpace/Campfire-2026/plugin/build/release/vcpkg_installed
cd /d "%~dp0"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static-md -DVCPKG_INSTALLED_DIR=%PKGS% -DVCPKG_MANIFEST_MODE=OFF -DCMAKE_PREFIX_PATH=%PKGS%/x64-windows-static-md "-DCMAKE_CXX_FLAGS=/permissive- /Zc:preprocessor /EHsc /MP -DWIN32_LEAN_AND_MEAN -DNOMINMAX -DUNICODE -D_UNICODE" || exit /b 1
cmake --build build || exit /b 1
