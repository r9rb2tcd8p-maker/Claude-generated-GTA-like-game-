#!/bin/bash
# Builds Palm Heat as a double-clickable macOS app. Needs only Xcode Command Line Tools (xcode-select --install).
set -e
cd "$(dirname "$0")"
ARCHS="${ARCHS:-$(uname -m)}"        # e.g. ARCHS="arm64 x86_64" for a universal build
ARCHFLAGS=""; for a in $ARCHS; do ARCHFLAGS="$ARCHFLAGS -arch $a"; done
FLAGS="-std=c++17 -O2 -Wall -Wno-deprecated-declarations -DGL_SILENCE_DEPRECATION $ARCHFLAGS -mmacosx-version-min=10.13"
OBJ=build/obj; APP=build/PalmHeat.app
rm -rf build; mkdir -p "$OBJ" "$APP/Contents/MacOS"
for f in world renderer game game_player game_draw; do
  echo "compiling $f.cpp"; clang++ $FLAGS -c $f.cpp -o "$OBJ/$f.o"
done
echo "compiling main.mm"; clang++ $FLAGS -fobjc-arc -x objective-c++ -c main.mm -o "$OBJ/main.o"
echo "linking"
clang++ $ARCHFLAGS "$OBJ"/*.o -o "$APP/Contents/MacOS/PalmHeat" -framework Cocoa -framework OpenGL -framework QuartzCore
cat > "$APP/Contents/Info.plist" <<'PL'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
<key>CFBundleName</key><string>Palm Heat</string>
<key>CFBundleDisplayName</key><string>Palm Heat</string>
<key>CFBundleExecutable</key><string>PalmHeat</string>
<key>CFBundleIdentifier</key><string>local.palmheat.game</string>
<key>CFBundlePackageType</key><string>APPL</string>
<key>CFBundleShortVersionString</key><string>1.0</string>
<key>LSMinimumSystemVersion</key><string>10.13</string>
<key>NSHighResolutionCapable</key><true/>
<key>NSPrincipalClass</key><string>NSApplication</string>
<key>LSApplicationCategoryType</key><string>public.app-category.games</string>
</dict></plist>
PL
codesign --force --deep -s - "$APP" >/dev/null 2>&1 || true
echo; echo "Built: $(pwd)/$APP"
echo "Run with:  open build/PalmHeat.app      (or ./build/PalmHeat.app/Contents/MacOS/PalmHeat to see logs)"
