# Fish Tycoon 2 – PS Vita port

A native PS Vita (C++/SDL2) re-implementation of Fish Tycoon 2 (Android v1.11.6).
It runs the game's own art, UI layouts, sounds and fonts, taken from **your own copy** of the
Android package, with the game logic rebuilt from the original code (see NOTES.md).
The .vpk itself contains no game assets.

## 1. Data (from your own APK)
    pip install UnityPy Pillow      # ffmpeg on PATH is optional (music → .ogg)
    python3 tools/ft2extract.py Fish_Tycoon_2.xapk ft2vita
Copy the resulting `ft2vita` folder to the Vita as `ux0:data/ft2vita/`.

## 2. Build the .vpk
Either install VitaSDK (https://vitasdk.org) and run

    mkdir build && cd build && cmake .. && make

or push this folder to a GitHub repo: the included workflow (.github/workflows/build.yml)
builds `ft2vita.vpk` in the cloud and attaches it as a downloadable artifact.

Desktop test build (Linux, SDL2 dev packages): `cmake -DDESKTOP=ON .. && make`,
then `./ft2vita --data <data folder>`.

## 3. Install
Install `ft2vita.vpk` with VitaShell. Saves go to `ux0:data/ft2vita/save/`.

## Controls
Touch screen works exactly like the phone game. Everything can also be done with buttons:
* Left stick / D-pad: move the hand cursor · ✕: tap (hold ✕ + move to drag fish and items)
* Right stick: scroll lists under the cursor
* L / R: previous / next tank (in Supplies: switch tabs)
* △: Supplies · □: Species chart · SELECT: My Items · START: options
* ◯: back / close
* SELECT on the title screen: quit the game and write a debug dump

## Saves, logs and dumps (ux0:data/ft2vita/save/)
* save.json + save.json.bak + save.json.bak2 – rolling backups; a damaged save is restored automatically.
* log.txt – log of the current session.
* crash_<time>.txt – written automatically at start-up if the previous session did not close cleanly.
* dump_<time>.txt – written when you quit with SELECT on the title screen (game state + recent log).

## What differs from the phone version
* Online features are removed: ads, Facebook, cloud saves, daily rewards, offers, in-app purchases.
  The Bank tab exchanges coins ⇄ gems instead.
* The store catalogue, prices and fish values came from the game's server (not in the APK);
  they are re-created in src/catalog.cpp and can be tuned there.
* Fish-store customers are simplified (static figures instead of Spine animation).
