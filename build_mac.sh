#!/bin/bash
set -e
cd "$(dirname "$0")"
cmake -B build -G Xcode
cmake --build build --config Release
echo "Done. Plugins were copied to ~/Library/Audio/Plug-Ins/"
