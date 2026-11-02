# Contributing

Repository: [F1j1tun1/Final-Project-CGF](https://github.com/F1j1tun1/Final-Project-CGF). The project requires C++17 and CMake 3.24+; the application uses OpenGL 3.3 Core.

Create a branch from the current main branch, save your changes, and open a pull request. Coordinate shared files with other contributors; do not overwrite someone else’s work or shared branch history.

```powershell
git switch -c feature/change-name
cmake --preset core-only
cmake --build --preset core-only --parallel
ctest --preset core-only
cmake --preset desktop
cmake --build --preset desktop --parallel
ctest --preset desktop
.\build\desktop\Debug\blackjack_forge.exe --smoke
```

`src/game/` contains pure game rules and seeded randomness; `src/graphics/` contains rendering; `src/main.cpp` contains the window, input, and UI. Invalid commands do not change state, and visual effects do not affect outcomes. See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the technical overview.

Add a meaningful test when changing rules. For graphics changes, inspect a visible frame, the window size, and the relevant scenario. In a pull request, list the commands you ran and their results. Preserve licenses and asset sources; do not add build outputs, binaries, temporary files, or secrets. Write documentation, UI text, and identifiers in English.
