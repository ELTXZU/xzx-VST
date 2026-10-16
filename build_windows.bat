@echo off
setlocal
title Building xzx VST
cd /d "%~dp0"

where cmake >nul 2>nul
if errorlevel 1 (
  echo CMake not found. Install it from https://cmake.org/download/ ^(tick "Add to PATH"^) and run this again.
  pause & exit /b 1
)

echo === Configuring (first run downloads JUCE) ===
cmake -B build -G "Visual Studio 17 2022" -A x64
if errorlevel 1 goto :err

echo === Building ===
cmake --build build --config Release --target XZX_VST3 --parallel
if errorlevel 1 goto :err

set "SRC=%~dp0build\XZX_artefacts\Release\VST3\xzx VST.vst3"
set "DST=%CommonProgramFiles%\VST3\xzx VST.vst3"

echo === Installing to %CommonProgramFiles%\VST3 ===
xcopy "%SRC%" "%DST%\" /E /I /Y >nul 2>nul
if errorlevel 1 (
  echo.
  echo Build OK, but couldn't copy to Program Files ^(needs admin^).
  echo Right-click this file ^> Run as administrator, or copy the folder manually:
  echo   %SRC%
  start "" explorer "%~dp0build\XZX_artefacts\Release\VST3"
) else (
  echo.
  echo Done! In FL Studio: Options ^> Manage plugins ^> Find installed plugins.
)
pause
exit /b 0

:err
echo.
echo Build failed. Make sure Visual Studio 2022 with "Desktop development with C++" is installed.
pause
exit /b 1
