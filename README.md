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

## License
 I actually encourage and fully allow this file to be used for any purpose especially as a base as you can use Claude or your own skills to generate/create even better and wilder projects like this, i would be happy to see this being used. :) 

## Features
What Claude generated is the following: 
1. Hijack cars and drive them around
2. Fully working police with a 5 star wanted level
3. You can eliminate people and the police which increases your wanted level
4. A few convenience stores that can be robbed using the "E" key (they give you around 200$ to 600$)
5. A larger town and a smaller village with a convenience store
6. A bank which is slightly buggy and i'm not sure if it can be robbed
7. NPC's that walk around and sometimes freak out.
8. 3 different weapons - Unarmed, Pistol and SMG
9. Basic graphics with buildings
10. Mini map (with zoom)
11. 2 different camera distance options (first-person isn't currently a part of the game)
12. I'm sorry, but no audio for now :( (coming very soon)
13. Aim and fire toggles as well as reload
14. Crouching, jumping and sprinting
15. Large empty area around the city/village with trees and flat terrain
16. working traffic lights

For now this is all, but from just one detailed prompt, i think it is very good. It does include very, very simple cube and Minecraft-like graphics, but i will REALLY, REALLY appreciate you checking out the game if you have a mac.
