# Palm Heat (macOS)

## Build & run
Needs only Xcode Command Line Tools (`xcode-select --install`). No other libraries.

    cd PalmHeat
    ./build.sh
    open build/PalmHeat.app

(Run `./build/PalmHeat.app/Contents/MacOS/PalmHeat` from Terminal to see log output.)
Universal binary: `ARCHS="arm64 x86_64" ./build.sh` (Claude generated this but i personally used the cd ~/Downloads/PalmHeat && chmod +x build.sh && ./build.sh 
compiling world.cpp command)

## Controls
WASD/arrows move, Shift sprint, Space jump (handbrake in car), C crouch, mouse look, LMB fire, RMB aim,
R reload, Q or 1-3 weapon, F enter/exit/carjack, E rob store/bank vault, V camera distance, Z look behind (car),
M minimap zoom, F5 save, F9 load, Esc pause (Q while paused quits), Ctrl+Cmd+F fullscreen.
Save file: ~/Library/Application Support/PalmHeat/save.txt

## Files
world.cpp / renderer.cpp / game.cpp   your originals (game.cpp has 3 small bug fixes)
game_player.cpp  player, weapons, camera, robbery, save/load, update loop (new)
game_draw.cpp    models, HUD, minimap, render (new)
main.mm          Cocoa window + OpenGL 3.2 core context (new)
test/            Linux headless stress test used for verification (not needed on Mac)
