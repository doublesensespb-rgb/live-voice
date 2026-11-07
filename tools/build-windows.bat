@echo off
rem One-click local Windows build (needs Visual Studio 2022 with C++ and CMake). JUCE 8.0.10 must be in .\JUCE
cd /d "%~dp0\.."
if not exist JUCE\CMakeLists.txt (
  echo JUCE not found. Download https://github.com/juce-framework/JUCE/archive/refs/tags/8.0.10.zip and unpack it to the folder JUCE
  pause
  exit /b 1
)
cmake -B build -A x64 || goto :err
cmake --build build --config Release --parallel --target LiveVoice_Standalone || goto :err
echo.
echo DONE: build\LiveVoice_artefacts\Release\Standalone\LIVE VOICE.exe
pause
exit /b 0
:err
echo BUILD FAILED
pause
exit /b 1
