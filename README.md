# Durams Sn  (AU + VST3)

Velocity-driven snare "re-player": load ONE snare one-shot, play it with any MIDI velocity and it
behaves like a drummer hitting with different force (level, brightness, attack, wires, body,
pitch tension, ring-out and tiny random variations change together).

## Build (needs CMake >= 3.22 and internet on first run - JUCE 8.0.6 is downloaded automatically)

### macOS (AU + VST3 + Standalone, universal arm64/x86_64)
    xcode-select --install          # once
    ./build_mac.sh
Plugins are copied to ~/Library/Audio/Plug-Ins/Components (AU) and ~/Library/Audio/Plug-Ins/VST3.
Then rescan plugins in your DAW. For Logic: run `auval -v aumu DrSn Drms`.

### Windows (VST3 + Standalone) - Visual Studio 2022 with "Desktop development with C++"
    build_windows.bat
The AU format exists only on macOS (Apple limitation).

## Controls
LEVEL, CURVE (velocity curve), RANGE (dynamic range), TONE, BRIGHT (how much soft hits get darker),
HUMAN (random variation per hit), PITCH, SNAP (stick attack), DECAY, WIRES, BODY, DRIVE.
Double-click a knob = reset. Hover/drag shows the value. Centre window: click or drop an audio file;
folder icon = load another file, play icon = audition.

## Engine self-test (no JUCE needed)
    g++ -std=c++17 -O1 -fsanitize=address,undefined Tests/engine_test.cpp -o engine_test && ./engine_test

## License note
JUCE is AGPLv3 / commercial. Distributing the built plugin publicly requires either releasing the
source under AGPLv3 or a JUCE commercial licence.
