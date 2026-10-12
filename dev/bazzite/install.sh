#!/bin/sh
# Install into ~/Games, add a desktop menu entry and a Steam library shortcut
# with artwork. Run from the extracted Bazzite package; reruns update in place
# and keep saves, which live outside the game folder.
set -eu
src=$(cd -- "$(dirname -- "$0")" && pwd)
dest=${1:-"$HOME/Games/super-mario-world-ii"}

if [ "$src" != "$dest" ]; then
    mkdir -p "$dest"
    cp -a "$src"/. "$dest"/
fi
chmod +x "$dest/play.sh" "$dest/world-warrior-smw"

apps="$HOME/.local/share/applications"
mkdir -p "$apps"
cat > "$apps/super-mario-world-ii.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=Super Mario World II
Comment=The World Warrior
Exec="$dest/play.sh" --fullscreen
Path=$dest
Icon=$dest/steam/icon.png
Categories=Game;
EOF

restart=
if pgrep -x steam >/dev/null 2>&1; then
    echo "Closing Steam to update its library..."
    steam -shutdown >/dev/null 2>&1 || true
    for _ in $(seq 60); do pgrep -x steam >/dev/null 2>&1 || break; sleep 1; done
    restart=1
fi
python3 "$dest/steam/steam_shortcut.py" "$dest"
if [ -n "$restart" ] && [ -z "${NO_STEAM_RESTART:-}" ]; then
    nohup steam >/dev/null 2>&1 &
fi
echo "Installed to $dest. Launch Super Mario World II from Steam or the app menu."
