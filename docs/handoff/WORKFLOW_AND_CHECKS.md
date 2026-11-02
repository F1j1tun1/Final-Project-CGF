# Workflow and verification after handoff

Run all commands below from the project root. The handoff branch is `codex/account-handoff`, code checkpoint `f7e9526`; `codex/theatre-polish` is the previous local development branch. Local `main` remains at `9504581`: continue on the current branch, do not merge it into `main`, and do not publish changes without the corresponding user request. Before working, check `git status`, the current branch, and recent commits; do not assume the old `main` is the latest game version.

## Environment and build

CMake 3.24+, MSVC from Visual Studio/Build Tools with the Desktop development with C++ component, and the Windows SDK are required. A working OpenGL 3.3 Core driver is needed for a window, including `--hidden`. If the compiler cannot be found, open Developer PowerShell. The project uses C++17; GLFW 3.4, GLM 1.0.1, and Dear ImGui 1.91.9b are already in `third_party/`. GLAD and stb_image_write come from vendored GLFW. CMake does not download dependencies; the full source package builds offline. An assistant account, plugins, and assistant skills are not required for the game or build.

Debug build with the app and both test suites:

```powershell
cmake --preset desktop
cmake --build --preset desktop --parallel 2
ctest --preset desktop --no-tests=error --timeout 120
.\build\desktop\Debug\blackjack_forge.exe
```

Rules and presentation only, without a window/GPU:

```powershell
cmake --preset core-only
cmake --build --preset core-only --parallel 2
ctest --preset core-only --no-tests=error --timeout 120
```

Release build for delivery, after configuring `desktop`:

```powershell
cmake --build build/desktop --config Release --parallel 2
ctest --test-dir build/desktop -C Release --output-on-failure --no-tests=error --timeout 120
$game = '.\build\desktop\Release\blackjack_forge.exe'
```

Presets use Debug by default. With the Visual Studio multi-config generator, choose a configuration with `--config` and `ctest -C`; `CMAKE_BUILD_TYPE=Release` alone does not switch it. With a single-config generator, the executable may be directly in `build/desktop/`. `scripts/run.ps1` builds, tests, and starts Debug; `scripts/run.ps1 -Release` does the same for Release. Do not move `build/` or its CMake cache between computers: configure again in a separate folder and preserve any needed results from the old build.

## Reproducible runs

`--help` lists current flags. Important behavior:

| Flag | Behavior |
| --- | --- |
| `--seed N` | Fixes the initial seed; without it, a normal launch gets a new seed |
| `--deck hands\|life\|coins` | Selects LAST LAUGH / ENCORE / GOLDEN FOOL for demo and initial selection |
| `--demo` | Skips the menu and starts the first hand |
| `--hidden --frames N` | Hidden window with a frame limit; a graphics context is still required |
| `--size WIDTHxHEIGHT` | Initial size from 1100×720 to 7680×4320 |
| `--autoplay` | Performs valid game actions to progress through states |
| `--smoke` | Enables hidden/autoplay, an input and resize scenario; sets 1200 frames and seed 42 if no seed is specified |
| `--capture-states DIR` | Saves reached states as PNGs and checks actual text metrics for all 24 relics |
| `--screenshot FILE` | Saves the final frame; without `--frames`, runs for only 3 frames |
| `--no-animation` | Skips the starting fade; card and reward animations remain |
| `--no-ambient-motion` | Stops background motion; game animations continue |
| `--assets DIR` | Specifies a different asset root |

Argument order matters: put `--frames 2400` **after** `--smoke`, otherwise smoke resets the limit to 1200. Smoke changes the window size itself; use hidden/demo/autoplay for a fixed size.

```powershell
& $game --smoke --frames 2400 --capture-states build/checks/smoke
& $game --hidden --demo --deck life --seed 42 --size 1100x720 --autoplay --frames 2400 --capture-states build/checks/small
& $game --hidden --demo --seed 42 --size 1100x900 --frames 3 --capture-states build/checks/tall
& $game --hidden --demo --seed 42 --size 1440x900 --no-ambient-motion --frames 300 --screenshot build/checks/table.png
```

These are example commands, not a record of checks already performed. Success means exit code 0, no OpenGL errors, and actual inspection of the required PNGs. Autoplay does not guarantee every relic or outcome; check which files were actually created. In smoke, Game Over remains on screen until the fade completes; `end-*` is the finished screen, and `end-fade-*` is the transition. `relic-reaction-*` shows an applied effect; ordinary `applied-relics-*` may be captured at the start of the step.

## What to check after a change

- `blackjack_rules` and `card_presentation` must pass. Do not change rules, card/shop RNG, hand evaluation, or AppliedEffect for a visual adjustment. A rules change needs a meaningful test.
- Inspect the UI at 1100×720, 1100×900, and 1440×900. After changing responsive breakpoints, also check their boundaries, such as widths 1324, 1664, and 1920. Diagnostics for all 24 relics check names, initial and used Second Chance states, long words, and the lower text boundary. They do not replace inspecting the frame.
- With four occupied slots, the name, rarity, and description stay visible beside each chip, without a shared background panel. Decimal bonuses such as `+0.6` are not split at the decimal point. Check Streak, Second Chance, and long names. Effect animation happens beneath its chip and shows the actual `+points`, `+coins`, or `+MULT` while keeping text readable.
- Check the menu and all three deck choices; COLLECTION with 24 items/filters; the shop with two offers and four occupied slots; purchases/sales; the dealer's hidden card; bust, reward, and Game Over with retry/revive/menu/quit.
- Space performs the current phase action or speeds up a step; Enter activates the selected button. Only the right mouse button rotates the camera; the left button does not move it. The wheel zooms, and double right-click resets the view. Panels must block input behind them. Check camera extremes after scene changes.
- To compare background motion, hold seed, frame, size, and card state constant. With Ambient motion off, compare identical arrival/reach/celebration/bust envelopes: these are separate gameplay animations. Do not claim FPS, no regressions, or a successful run without measurement/results.

Do not rerun the full suite after every small reversible change: start with the relevant build and specific frame/scenario; expand checks when rules or UI contracts change, or when there is an error or new risk.

## Delivery to Desktop and preservation

First create a **new** staging folder and install Release there (`cmake --install build/desktop --config Release --prefix build/delivery-new`). Rename the installed `blackjack_forge.exe` to `JesterJack.exe`; do not rename the CMake target. Check that it runs from staging, including from a different working directory. Keep `assets/`, fonts, and licenses next to the executable.

The baseline showcase copy for this version contains 54 files: the executable; top-level `CMakeLists.txt`, `CMakePresets.json`, `CONTRIBUTING.md`, short `README.md`, and `THIRD_PARTY_NOTICES.md`; `assets/` (17), `licenses/` (5), `scripts/` (1), `src/` (12), `tests/` (2); and three regular technical documents plus eight images in `docs/` (11). **Also** copy `third_party/glfw`, `third_party/glm`, and `third_party/imgui` in full, or the source cannot be rebuilt. A lone `cmake --install` does not include source files or vendored dependencies. The count of 54 describes this composition; it does not mean required new files should be removed to match the count.

Do not copy `.git`, build folders, logs, temporary captures, handoff documents, account settings, or secrets into the clean showcase folder. Do not copy all of `docs/` indiscriminately: `docs/handoff/` is for continued development. Preserve third-party licenses and upstream files. Keep the showcase README short: title, rules/controls, launch, and build.

Before replacing the Desktop copy, preserve the previous folder/ZIP and manifest in a separate archive **outside the repository and showcase folder**. Compare the file list and SHA-256 hashes of the new staging copy and destination; count delivery complete only when they match. Do not delete the old copy or archive before verifying the new one. Example for generating a relative manifest, run separately on staging and destination:

```powershell
function Get-DeliveryHashes([string]$Root) {
    $deliveryBase = (Resolve-Path -LiteralPath $Root).Path.TrimEnd('\')
    Get-ChildItem -LiteralPath $deliveryBase -Recurse -File | ForEach-Object {
        [pscustomobject]@{
            Path = $_.FullName.Substring($deliveryBase.Length + 1)
            SHA256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
        }
    } | Sort-Object Path
}
$before = Get-DeliveryHashes 'build/delivery-new'
$deliveryTarget = Join-Path ([Environment]::GetFolderPath('Desktop')) 'Jester Jack'
$after = Get-DeliveryHashes $deliveryTarget
Compare-Object $before $after -Property Path,SHA256
```

No output from the last command means the paths and hashes match. Save the manifest and verification results beside the backup; do not add them to the showcase folder. Verify paths in advance. For recursive operations, use a single PowerShell session, `-LiteralPath`, and an explicit target directory; do not construct deletion through another shell or an unchecked variable.

## Collaboration and Git

One person owns a file at a time. A useful division: coordinator — `src/main.cpp` (UI, input, timers, makeScene); graphics — `Renderer.cpp`/`Renderer.hpp` and shaders; room — `RendererRoom.inl`; documentation — agreed Markdown files. Assign rules/Presentation/tests to a separate owner only when needed. Coordinate `Scene` changes, shared headers, CMake, and new assets in advance; do not edit the same file concurrently.

Before a shared build, each contributor confirms that their files are saved and frozen. Then run one agreed build, verify metrics and relevant frames, fix any defect, and repeat the affected checks. After the final freeze, update assets beside the executable, documentation, and only then staging/Desktop. Do not commit an intermediate mix of an old executable and new assets.

Make small, clear commits; review the diff and `git diff --check` first. Do not overwrite someone else's changes, use `reset --hard` or `clean`, force-push, rewrite history, or delete backups as a way to “fix” state. Do not commit build outputs, binaries, temporary materials, or secrets. Keeping the current branch, merging into main, and pushing are separate actions; a request for a local change does not by itself authorize publishing.

User preferences: brief updates in Russian, more action and fewer comments. Continue already authorized work without asking again; clarify only substantial ambiguity or an action for which authorization is actually missing. Skills, parallel contributors, and tools are optional and selected based on the task; do not transfer old account settings/tokens into the project. A new contributor needs only the source, these documents, and an available build environment.
