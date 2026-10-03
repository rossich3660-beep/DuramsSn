@echo off
cd /d "%~dp0"
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
echo Done. VST3 is copied to C:\Program Files\Common Files\VST3 (run as Administrator if copy failed)
pause
