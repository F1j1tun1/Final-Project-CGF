# Jester Jack

A single-player blackjack game with endless rounds and relics. Built with C++17 and OpenGL 3.3 Core.

## Getting started

In a packaged build, open `JesterJack.exe`. Keep the `assets/` folder next to it. The game requires Windows with an OpenGL 3.3 Core driver.

## Rules and controls

Press START and choose a starting deck:

| Deck | Advantage |
| --- | --- |
| LAST LAUGH | Six hands per round instead of five |
| ENCORE | Once per run, revive after a failed round while keeping coins and relics |
| GOLDEN FOOL | Start a run with 10 coins |

Reach the round target: 30 points at first, then +8 per round, up to 86. A winning hand awards points equal to its total and 4 coins, plus relic effects. A losing hand or push uses a hand. Meeting the target opens the shop with two offers; the run ends if you run out of hands first.

In COLLECTION, find all 40 relics: 14 Common, 15 Rare, and 11 Legendary. Their prices are 3, 6, and 10 coins, respectively. You can hold up to eight relics and sell them for half their price, rounded down. Shop rarity weights are 50/36/14; Common is white, Rare is blue, and Legendary is gold. A new run starts with no chips: each physical chip appears when its relic is bought and disappears when sold. If your inventory is full, you can replace a selected relic. A natural blackjack scores 21 points without relics; the Common Blackjack Bonus costs 3 coins and adds 5 points to a natural-blackjack win.

| Input | Action |
| --- | --- |
| ← / → on deck selection | Switch decks |
| H | Hit |
| S | Stand; when a bust rescue is pending, accept the loss |
| 1–8 / click a chip | Select a relic to inspect; this does not use a charge |
| A / ACTIVATE | Use the selected available active ability |
| Space | Main action: select a deck, deal, stand, continue, revive, or retry |
| Click / Space during an animation | Speed up the current dealing or reward step |
| Enter | Activate the highlighted button |
| F1 | Settings |
| Escape | Skip the casino entrance, close an open window, or return to the menu |
| Right-drag on an empty table / scroll wheel | Orbit / zoom the camera |
| Double right-click on an empty table | Reset the camera |

Rewind removes the last drawn card, Redraw replaces it, Forked Path lets you choose one of two cards, and Lifeline rescues you from a bust before settlement. Charges and conditions appear in the selected chip's description; confirm a Forked Path choice by clicking one of the cards.

Full rules are in HOW TO PLAY; animation speed and Cinematic entrance are in OPTIONS. Turning off Cinematic entrance replaces camera movement with a short fade. Buttons also work with the mouse. Exit with EXIT or QUIT GAME. See the [architecture guide](docs/ARCHITECTURE.md), the [full relic catalog and limits](docs/catalog-balance.md), and the [progression balance report](docs/balance-results.md).

## Audio

Audio works on Windows: MP3 files use the built-in system decoder, so nothing needs to be installed. Keep `assets/audio/` next to the game. If the folder is missing or a file is damaged, that sound is skipped. On other systems, the game runs without audio.

The main theme loops in the menu. It switches to soundtracks when you take a seat (TAKE A SEAT) and restarts when you return to the menu. Soundtracks (`assets/audio/music/soundtrack_*.mp3`) play in a shuffled loop and do not end; add `soundtrack_6.mp3` and later tracks to the same folder to extend it. Cards, chips, wins, losses, and UI sounds follow the table in the [architecture guide](docs/ARCHITECTURE.md#audio); button-hover sounds play only in the menu. A recording tied to its position plays as the car drives past the main-menu street. A [five-piece jazz ensemble](docs/ARCHITECTURE.md#jazz-ensemble) plays behind the dealer in time with the current soundtrack. Volume levels are defined in `audio::Levels` (`src/audio/Mixer.hpp`).

## Build on Windows

You need CMake 3.24+ and Visual Studio 2022/2026 or Build Tools with MSVC, the Windows SDK, and the Desktop development with C++ component. Dependencies are included in `third_party/`.

From a terminal in the folder containing `CMakeLists.txt`:

```powershell
cmake --preset desktop
cmake --build --preset desktop --parallel
ctest --preset desktop
.\build\desktop\Debug\blackjack_forge.exe
```

If CMake cannot find MSVC, use Developer PowerShell. The built executable is `blackjack_forge.exe`; the game is called Jester Jack.

To build only the rules, animation sequence, and their tests, without a window or GPU:

```powershell
cmake --preset core-only
cmake --build --preset core-only --parallel
ctest --preset core-only
```

Library and font licenses are kept in `third_party/`, `assets/fonts/`, and the packaged build's `licenses/` folder. Third-party sources are listed in `THIRD_PARTY_NOTICES.md`. Keep the assets and licenses when copying the game.