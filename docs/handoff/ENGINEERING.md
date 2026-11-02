# Jester Jack: engineering map for continued work

Handoff snapshot: `f7e9526`, October 22, 2026. This maps the existing implementation; it is not a list of new requirements. If there is a conflict, current source and build configuration take priority; old images and descriptions must not silently replace code behavior. The project version in `CMakeLists.txt` is `0.3.0`; the CMake target/executable is still named `blackjack_forge`.

## 1. Areas of responsibility

| Path | Responsibility and main entry points |
| --- | --- |
| `src/game/Game.hpp`, `Game.cpp` | Rules, two random-number generators, relic catalog, shop, and run. `Game::deal/hit/stand/advance/buy/sell/leaveShop/revive/restart`, `Game::state`, `evaluateHand`, `relicCatalog`. No OpenGL, ImGui, files, or clocks. |
| `src/presentation/Presentation.hpp`, `Presentation.cpp` | Queue of intermediate snapshots. `Presentation::begin/update/accelerate/reset`, `visible`, card/flip progress, reward steps, and effect pulses. Does not call game commands or change RNG. |
| `src/main.cpp` | Window/ImGui creation, time, input, commands, menu, shop, collection, deck selection, HUD, transitions, captures/smoke. `Commands`, `Session`, `HandFeedback`, and `makeScene` connect the other layers. |
| `src/graphics/Renderer.hpp`, `Renderer.cpp` | Public `Scene/VisualCard/RenderSettings`, GPU resources, procedural meshes/textures, dealer, table, casino, sedan, and effects. External entry point: `Renderer::render`. |
| `src/graphics/RendererRoom.inl` | Part of `Renderer::Impl`, included directly by `Renderer.cpp`. `drawRoom`, `roomPortal`, `roomColumn`, `roomCurtainRail`, `roomLamp`, `roomLampBrightness`, `roomMainLegs`. Not a standalone compilation unit. |
| `src/graphics/ArtTexture.hpp` | RAII loader for uncompressed TGA files into `GL_TEXTURE_2D`, including row orientation and restoration of OpenGL unpack state. |
| `assets/shaders/table.vert`, `table.frag` | Custom main GLSL 330 pipeline, normal matrix, UVs, materials, lighting, and natural distance fade. |
| `assets/shaders/sky.vert`, `sky.frag` | Full-screen procedural evening sky for the main menu. |
| `src/presentation/CardSuit.hpp` | Vector card suits for ImGui: `presentation::drawCardSuit`; independent of font Unicode glyphs. |
| `tests/game_tests.cpp`, `tests/presentation_tests.cpp` | CPU checks for rules and presentation. CTest names: `blackjack_rules`, `card_presentation`. |

Detailed game rules and the catalog of all 24 effects are already described in `docs/ARCHITECTURE.md`. Do not duplicate reward calculations in the UI or shader.

## 2. How a player action flows

```text
button / key
    → Commands::apply: at most one accepted command per frame
    → Game immediately receives the resulting Snapshot
    → Presentation::begin(before, after, Action)
    → Presentation::update(dt, speed) returns visible() and current-step progress
    → UI draws the visible state; makeScene builds gfx::Scene
    → Renderer draws 3D; ImGui is drawn over it
```

`Snapshot` is a value containing state: cards, `Phase/Outcome`, round target and score, hands remaining, starting deck, coins, streak, `owned/offers`, revival resource, and `ScoreBreakdown`. `ScoreBreakdown::appliedEffects` contains the actual relic contributions (`points/coins/multiplierBonus`). Those entries drive the sequential chip highlights; the UI does not infer activations from a relic's name or description.

`Game` and `Presentation` store different stages of one action. During a deal, the actual `Game::state()` may already contain the dealer's final result, which must not yet be shown to the player. For the table, use `timeline.visible()`, `settledState()`, and the dealer-reveal methods instead of assigning the final snapshot directly. `Commands::blocked/used` prevent duplicate commands and input while the queue is busy.

The Presentation queue contains `PlayerCard`, `DealerCard`, `Flip`, `Rescue`, `RewardBase`, `RewardEffect`, and `RewardTotal`. `accelerate()` completes the current step rather than executing another game command. `update` consumes leftover `dt` between steps. At natural speed, a card takes 0.55 s, flip/base takes 0.4 s, rescue/effect takes 0.5 s, and final takes 0.6 s; the slider shortens each step to 0.07 s. Invalid or non-positive `dt` is ignored.

Important rule invariants:

- A fresh 52-card deck is shuffled for every hand; Fisher–Yates uses `boundedRandom` with rejection sampling. `cardRandom_` and `shopRandom_` are independent. Decorative animation and scene time must never call the game RNG.
- The hand outcome is calculated once in Game. A natural blackjack by itself awards 21 points; the extra bonus belongs to the Blackjack Bonus relic.
- Losing an individual hand uses a hand; the run ends when there are no hands left and the target has not been reached. Meeting the round target takes priority.
- At most four distinct relics are equipped. Buying/selling does not refresh the current shop pair. A sold relic may appear on the next visit; half of its cost is returned, rounded down.
- `revive` retries the same round without rewinding RNG or losing coins/relics. `restart` starts a separate new run.

`Session` stores only application state: open screens, transition, deck selection, animation speed, hover/reaction, and game-over timer. `Session::updateBustReaction` generates a 0.8 s envelope for a newly visible bust. `HandFeedback::observe` displays the actual total delta for 1.5 s after a card is added, including ace recalculation; a new hand/round/seed, rescue, or hidden screen resets the hint. Runs are not currently saved to disk.

## 3. Scene contract and coordinate system

Scene units are arbitrary, not meters. `+Y` points up, and the table is around `Y=0`; at zero yaw the camera is on the `+Z` side, the player is closer to `+Z`, and the dealer is at `−Z`. `+X` points right across the table.

| Element | Current reference |
| --- | --- |
| Room floor | Top of main surface at `Y≈−0.775`. |
| Main table | Center at X/Z `(0,0)`, outer radii 4.2 and 3.0; far edge `Z≈−3.0`. Top of green felt `Y≈−0.005`. |
| Card hands | Dealer `Z=−1.12`, player `Z=+1.12`. Card size in X/Z: `0.74×1.08`, thickness 0.045. Normal X slot spacing 0.88; a long hand compresses to a span of 5.65. |
| Dealer | Root Z `−3.68`; coat transform `(0,−0.32+breath,−3.68)` with scale `(0.86,1.02,0.92)`. This is behind the table, not on the felt. Legs and shoes reach the floor. |
| Dealer's head | Center near `(0,1.54,−3.665)`. Separate `leadPorcelainShape` is a narrow, thin shell without a protruding nose; do not replace it with the old shared `porcelainShape`. |
| Resting hands | Wrists near `(±0.83,0.082,−2.89)`, fingers rest at the far edge. Card launch point is `(±0.83,0.12,−2.55)`. |

`VisualCard.rank` is 1..13; `suit` is 0 Clubs / 1 Diamonds / 2 Hearts / 3 Spades. `arrival=0` means starting from the dealer's hand; `arrival=1` means the table slot. `flip` controls the face-card reveal; a face-down `faceDown` card remains a card back. `makeScene` assigns the progress to the last visible card in the relevant hand and a separate flip to the dealer's second card.

Other Scene fields:

- `menu/time`: scene selection and decorative time;
- `coins`: height of decorative chip stacks;
- `dealerReach/dealingToPlayer`: dealing pose;
- `celebration/legendary`: prepared win and Legendary highlight envelopes;
- `bustReaction`: 1→0 envelope, with no outcome calculation in Renderer;
- `deckDesign`: 0 hands, 1 life, 2 coins;
- `tableViewport`: the **framing area**, normalized `left/top/width/height` from the UI's upper-left corner. It does not crop the world with a separate viewport.

`cardPosition` interpolates the source and slot with an added arc; the final 22% is used for landing. `drawCard` combines a physical flip with a lift so the card does not pass through the felt. `drawEffects` draws trails, landing rings, dust, and sparks with blending and depth writes disabled. Do not tie these effects to frame rate or the card generator.

## 4. Camera, full framebuffer, and OpenGL state

Defaults are defined **only** in `RenderSettings` (`Renderer.hpp`): yaw 0°, pitch 32°, distance 9.1, intensity 1.2, and textures/lighting/animation enabled. `resetCamera` in `main.cpp` reads those defaults and resets yaw/pitch/distance only; it does not change lighting, textures, or speed. User input is limited to yaw ±35°, pitch 32..68°, distance 8..14. Renderer’s internal safety limits are wider: pitch 20..85°, distance 5..18.

The game `lookAt` points to `(0,0.60,−0.50)`, up `(0,1,0)`. Perspective is FOV 43°, near 0.1, far 60. First, `Renderer::Impl::render` converts `tableViewport` into the source pixel rectangle `(L,B,w,h)` and builds the perspective from its aspect ratio. Then it maps that same projection into the full framebuffer `(W,H)`:

```text
sx = w/W                 ox = (2L+w)/W − 1
sy = h/H                 oy = (2B+h)/H − 1
for each column c:
    P[c][0] = sx * P[c][0] + ox * P[c][3]
    P[c][1] = sy * P[c][1] + oy * P[c][3]
```

GLM stores matrices by column; the W row remains unchanged. `glViewport(0,0,W,H)` draws the room across the full frame while preserving the previous pixel positions of cards and table. **Do not restore `glViewport(L,B,w,h)` for the room:** rectangular floor edges would appear beneath the transparent HUD. The shader edge vignette was removed; subtle top/bottom UI shading is now in `drawGameplayShading`, while the distant floor fades to black by world-space distance.

The menu does not use this off-axis correction: the camera at `(0,4.8,menuDistance)` looks toward `(0,1.55,0)`, where `menuDistance=15.5*max(1,1.45/aspect)`. `drawSky` renders a full-screen triangle without depth testing, then restores depth testing/writes.

At the start of rendering, depth test `GL_LESS` and depth writes are restored; blending/culling/scissor are disabled and the full framebuffer is cleared. The room has black clear/fog; the menu has separate subdued fog and 80% of main light intensity. `draw` passes the model and `transpose(inverse(mat3(model)))`, material, UV scale, color, opacity/specular/emission/velvet. Matrices are passed with `GL_FALSE`, not transposed as in older lecture examples.

`Program`, `Mesh`, `Textures`, and `ArtTexture` own resources through RAII. Object order in `main` keeps the GLFW context alive until GPU resources are destroyed. A mesh vertex has position3 + normal3 + UV2; VAO/VBO/EBO and static indices are created at construction. Only the dealer's sleeve updates vertices/normals through `updateLeadSleeve` and `glBufferSubData`, reusing the CPU vector and existing EBO. Do not add `glGen*`, texture loading, or a new dynamic mesh to the per-frame path.

## 5. Current art and material contract

The direction is a mysterious theater, without horror. The dealer keeps an adult, narrow silhouette, a nose-free porcelain mask, a purple-burgundy coat, muted gold, and lace. Do not bring back a large round head, oversized spherical shoulders, upright horns, or long dark tubes at the back. Change body/hand positions relative to the table or the current head shape only after separate agreement.

`drawDealer` has no idle head or hat bobbing. It has quiet breathing, a limited gaze toward a flying card, a short lean on `bustReaction`, and a small left-hand win gesture only outside an active hand. The arm passes through the elbow via two Hermite segments in `leadSleeveShape`; do not replace it with one curve that misses the elbow.

| `uMaterial` in `table.frag` | Purpose |
| --- | --- |
| 0 | Ordinary material/texture; a subtle felt wear effect is added for room+velvet. |
| 1 | Shared porcelain with eye cutouts. |
| 2 / 4 | Fabric / former two-tone jacket; some shared resources are retained even if the current room does not use them. |
| 3 | Checkerboard floor in world XZ, seams, and distance fade. |
| 5 | Checkered fabric on the soft hat. |
| 6 / 10 | Stripes / sleeve with dark forearm and trim. |
| 7 | Lace with small eyelets using discard. |
| 8 | Dealer mask with asymmetric coloring. |
| 9 | Coat: burgundy from `uColor`, violet from the shader, black lapels and belt. |

The dealer's primary palette is set in `drawDealer`; the coat's violet half, dark forearm color, and some trim are also assigned in `table.frag`. Check both files when changing the palette. Do not leave the old broad shared mask on the dealer or move the hood forward: these previously caused black geometry-intersection artifacts.

The room in `RendererRoom.inl` has an open central arch and further portals at Z −6.5 / −10.8 / −16; paired burgundy and purple curtains, columns, rails/rings, and candles. There are currently **no** background characters, extra game tables, walls/ceiling, or masks scattered on the floor. Lamps are symmetrical at `(±4.7,3.1,−5.75)`; candles at X ±5.45, Z −2.6. Local shader light positions must match the geometry.

`roomLampBrightness` is the single source of soft, occasional dimming: period 31 s, dips around 12 and 13.3 s, no sharp strobe. It returns 1 when `animate=false`. Lamp materials and `uLampBrightness` use the same value; do not update only one path.

## 6. Main menu: casino and sedan

`drawCasino` uses a shared building/street matrix: translate `(2.2,0,−3)`, rotate Y by `−0.70`, scale `(1.35,1.12,1.12)`. Road, curb, sidewalk, steps, and sedan must retain this shared basis. The casino uses custom 3D geometry: facade with a right annex, cornices, deep window frames, warm glass, canopy, entrance, and a round JJ medallion. Do not replace it with an image or webpage.

`sky.frag` creates subdued twilight with a low, soft sun; saturation and brightness have already been reduced. `drawMenu` adds its own translucent background on the left for title/button readability. Preserving it is a separate condition; do not confuse it with the removed game-world frame.

`drawMenuSedan` is a complete dark sedan with body, low hood/trunk, roof, windows/pillars, four wheels, headlights, and tail lights. Static meshes are created in advance. Movement depends entirely on `Scene.time`: `phase=fmod(max(time,0),32)`; it passes during phase 0..9.4 s from local X −18 to +18, then pauses for 22.6 s. The car's local Z is 6.13. With Ambient motion disabled, it stays outside the visible area. The game RNG is not used.

For a frame with the car, use a time around 5–5.5 s; for an empty road, use 16 s. Headlights use emissive geometry and subtle additive patches on the road. This pass saves and restores blend enable, RGB/alpha blend functions/equations, and depth writes; do not add opaque cones or leave altered state for ImGui.

## 7. Assets, originals, and fonts

Runtime art in the source:

| File | Format and use |
| --- | --- |
| `assets/art/relics.tga` | 1536×1024, 32-bit; 6×4 atlas, order follows enum `bj::Relic`. |
| `assets/art/deck-hands.tga` | 1024×1536, 32-bit; LAST LAUGH. |
| `assets/art/deck-life.tga` | 1024×1536, 32-bit; ENCORE. |
| `assets/art/deck-coins.tga` | 1024×1536, 32-bit; GOLDEN FOOL. |
| `assets/JesterJack.png`, `JesterJack.ico` | Application icon; Windows resource 101 is defined in `src/windows.rc.in`. |

`ArtAssets` loads TGA files for UI; `Renderer::Impl` uses the same card backs with `load(path,true)` for 3D. Normal `ArtTexture::load` defines UV `(0,0)` as the upper-left corner; `makeTopPlane` has a different V orientation, so this flip is required. Do not flip the card backs a second time in the UI.

In `drawChip`, the crop follows the actual atlas: `col=id%6`, `row=id/6`, `rowTop={8,252,500,748}`, plus the cell inset. Simply dividing into four equal rows produces incorrect crops. When replacing the atlas, preserve Relic order or change the mapping too and check all 24 images.

`ArtTexture` supports only uncompressed true-colour TGA type 2, 24/32 bit, without a color map/interleaving, up to 8192×8192. Renaming a PNG to `.tga` does not convert it. Missing required UI art is a startup error; the renderer can show a diagnostic procedural fallback for a card back. Card fronts, suits/ranks, felt, and some decor are generated in C++.

The current source tree has **no separate PNG originals/editable projects or generation-prompt manifest** for the four TGA files. The existing TGAs are sufficient to run and build. For future editing, source images, user references, and generation history must be provided separately; do not treat a temporary file or old account history as repository content. Record the location of such an additional package in the shared handoff once it is created. Sources/licenses for original illustrations are also not documented in a separate asset manifest.

Fonts are included in full and do not depend on Windows fonts:

- `assets/fonts/CormorantGaramond-Regular.ttf`, `CormorantGaramond-SemiBold.ttf`: Christian Thalmann / Cormorant Project, SIL OFL 1.1; source pinned to commit `bcf5515652e4d99eb3e42e41ed5f4285a315a175`.
- `assets/fonts/SourceSans3-Regular.ttf`, `SourceSans3-Semibold.ttf`: Adobe, SIL OFL 1.1, Reserved Font Name Source; source pinned to commit `87b37a2daaed80fcb8e8ccb0085c4d72ddade12e`.
- Exact upstream URLs, SHA-256 values, and licenses are in `assets/fonts/README.md`, `OFL-CormorantGaramond.txt`, and `OFL-SourceSans3.txt`. Do not replace these entries with unverified newer versions.

## 8. UI: edit locations and accepted sizes

All screens are in `src/main.cpp`; line numbers quickly become stale, so search by function. `Interface` creates ImGui with `imgui.ini` disabled and keyboard navigation enabled (`ImGuiConfigFlags_NavEnableKeyboard`). The atlas uses OversampleH=3/V=2.

| `Interface` role | Font / loaded size |
| --- | --- |
| `io.FontDefault` | Source Sans 3 Regular, 22 |
| `small` | Source Sans 3 Regular, 18 |
| `button` | Source Sans 3 Semibold, 26 |
| `menuButton` | Source Sans 3 Semibold, 34 |
| `number` | Source Sans 3 Semibold, 46 |
| `largeNumber` | Source Sans 3 Semibold, 104 |
| `heading` | Cormorant Garamond SemiBold, 36 |
| `title` | Cormorant Garamond SemiBold, 200 |

These are atlas sizes, not a guarantee that every draw call uses the same size. `drawMenu` reduces the title if there is not enough width; dealer labels use 64, the hand-change hint 34, the suit is drawn as a 32-pixel vector, and the rarity badge is 26. The main hand total is 104 and shows the actual bust total, not zero reward.

Main edit points:

- `menuLayout/drawMenu`: top-left title, vertical START/OPTIONS/COLLECTION/EXIT. Button width 350; height 64 when window height <800, otherwise 76; step 76/88. Keep the left overlay.
- `tableLayout`: sidebar `clamp(width*.24,280,340)`, content X=sidebar+24, right margin 20, action Y=height−154. `boardY` comes from `relicRowStyle`; `boardHeight=height−boardY−110`. These values also define Scene framing.
- `relicRowStyle`: for column width <235 — radius38/name22/body18/rarity16/height208; <320 — 44/26/20/18/208; otherwise 44/28/22/18/192. `drawRelicStrip` shows the name, rarity, and full description beside each chip. `wrapRelicWords/verifyRelicTextLayout` protect wrapping at the minimum window size.
- `drawRelicStrip`, `Session::updateRelicReaction`: the `AppliedEffect` contribution raises the relevant chip, shows rings, and briefly displays POINTS/MULT/COIN. Use Presentation timing instead of a separate bonus calculation.
- `drawSidebar`: target, score, hands, coins; detailed hand reward only at height >=820 so 720 remains usable.
- `controlsLeft/drawTable/drawHandValue`: HIT 190×78 and STAND 250×78 with 16 spacing; separate total width 210 on the right. At a narrow window, shift controls slightly left for clearance. Do not restore a wide text panel along the bottom.
- `drawShop`: two offers and four sale slots; `drawCollection`: all 24 chips/filters; `deckLayout/drawDeckSelection/browseDeck`: three-position carousel; `drawGameOver`: separate screen after fade.
- `beginPanel`, `theatreButton`, `rarityBadge`, `drawChip`: shared styles. Non-interactive labels use `ImGuiWindowFlags_NoInputs`; otherwise an invisible text panel may intercept HIT/STAND or camera drag.
- `drawSettings/resetCamera`, main input block: overlays must block screens/camera behind them. Escape first closes the current overlay. Do not duplicate keyboard and mouse commands in one frame.

After changing fonts, check more than the large title: long chip names/descriptions, all three rarities, two shop prices, four occupied slots, and the narrowest supported size. Do not fix one line by shrinking all text.

## 9. Build dependencies and machine setup

Requires CMake 3.24+, a C/C++17 toolchain, and an OpenGL 3.3 Core driver. On Windows: MSVC/Windows SDK/Desktop development with C++; no compiler is needed on the runtime machine. GLFW/GLM/ImGui source is included; npm, Python, a separate texture SDK, and network downloads during configure are not required.

Versions are pinned in `THIRD_PARTY_NOTICES.md`: GLFW 3.4, GLM 1.0.1, Dear ImGui 1.91.9b. GLAD comes from `third_party/glfw/deps/glad/gl.h`, the loader is `src/graphics/glad.cpp`, and GLFW's `stb_image_write.h` is used only for PNG captures. Do not update GLFW separately from GLAD without checking compatibility. Preserve upstream licenses.

`BJ_BUILD_APP=ON` and `BUILD_TESTING=ON` are desktop defaults. `find_package(OpenGL REQUIRED)` is only inside the app branch. `core-only` disables the window/graphics dependencies and builds the same rules/Presentation with tests. CMake uses C++17 without extensions, MSVC `/W4 /permissive- /utf-8 /EHsc`, and the static MSVC CRT (`MultiThreaded`/`MultiThreadedDebug`).

```powershell
cmake --preset desktop
cmake --build --preset desktop --parallel
ctest --preset desktop

cmake --preset core-only
cmake --build --preset core-only --parallel
ctest --preset core-only
```

The preset defaults to Debug; choose Release separately with Visual Studio's multi-config generator:

```powershell
cmake --build build/desktop --config Release --parallel 1
ctest --test-dir build/desktop -C Release --output-on-failure
cmake --install build/desktop --config Release --prefix dist/JesterJack
```

With a single-config generator, the executable may be directly in the build directory, and `CMAKE_BUILD_TYPE` is set during configure. `scripts/run.ps1` selects Debug or Release itself (`-Release`), builds, runs CTest, and locates both executable layouts. `.vscode/tasks.json/launch.json` use CMake Tools and the current launch target, with no personal paths.

`blackjack_assets` copies all of `assets/` beside the executable during build; install also copies notices and dependency licenses. Do not assume a changed source shader was automatically updated in an already-built portable folder. At launch, the root assets directory is searched beside the executable, then in the working directory; `--assets DIR` overrides it. The runtime target name is `blackjack_forge.exe`; the distributed copy may be named `JesterJack.exe`. Current assets and licenses must remain beside it.

## 10. Verifying graphics changes and handoff limitations

Preparing this document did not run a build or tests. The list below is a reproducible verification route; use the log/shared handoff for actual results from a particular delivery instead of treating the presence of a command as proof it was run.

Minimum size is **1100×720**; primary size is **1440×900**. Normal window: 1440×900; GLFW context 3.3 Core, MSAA 4, VSync enabled for visible windows and disabled for hidden runs. Check the actual framebuffer, not only ImGui logical coordinates; DPI differences and a different GPU/OS require separate verification.

```powershell
# Menu with the sedan near the fifth second; no autoplay.
.\build\desktop\Release\blackjack_forge.exe --hidden --seed 42 --frames 300 --size 1440x900 --screenshot build/qa/menu-car.png

# Static table after the first deal, without decorative cycles.
.\build\desktop\Release\blackjack_forge.exe --demo --hidden --seed 42 --no-ambient-motion --frames 240 --size 1100x720 --screenshot build/qa/table-small.png

# Full hidden integration path with automatic captures and resize.
.\build\desktop\Release\blackjack_forge.exe --smoke --frames 1200 --capture-states build/qa/smoke
```

The CLI parses arguments in order: put `--frames` after `--smoke` to override its 1200-frame default. `--screenshot` without frames selects only three frames. Hidden+frames uses `dt=1/60` and `sceneTime=frame/60`; a normal launch uses GLFW time. `--no-animation` disables only the starting fade, **not** the deal queue; `--no-ambient-motion` disables decorative cycles but not rules or Presentation. `--demo` deals the first hand immediately; `--deck hands|life|coins`, `--seed N`, `--autoplay`, and `--assets DIR` help reproduce scenarios.

Visual acceptance after a relevant change:

1. Menu at both sizes: title/all buttons fit; the building shows two faces; steps/curb run parallel to the road; the sedan passes and disappears during its pause; headlights produce no opaque triangles or blend-state leak.
2. Table: current composition and card positions remain; the world continues behind the top of the HUD and bottom buttons without a rectangular frame. Hands rest on the far edge, torso is behind it, mask/hood/collar do not intersect.
3. Deal/flip/landing, blackjack, ordinary win, bust, rescue, and ace recalculation. Bust total shows the actual 22+; delta can be negative. Hints do not cover buttons.
4. Four chips, long descriptions, Common/Rare/Legendary, sequential activations, and final reward. Shop with 0/4 and 4/4 slots, sales and purchases; COLLECTION/filters; all three card backs at both sizes.
5. Camera: yaw−35/pitch32/distance8 and yaw35/pitch68/distance14; cards remain visible, do not cut through felt/head, and floor has no hard edge. Check RMB/wheel/double-RMB and blocking behind SETTINGS/RULES.
6. Ambient motion off: no decorative sedan/idle bob, lamps are steady; game remains playable. Lighting/Textures/Light settings work, and reset camera does not reset them.
7. Close/relaunch from another working directory, resize, current assets beside executable. Capture path is created by the app; missing art/font and `glGetError` produce diagnosable errors.

`docs/images/` contains portable illustrations of the project state. `build/**` and automatically generated captures are Git-ignored and are not required source. An old successful screenshot does not prove that the current binary/asset copy matches the source.

Course mapping is detailed in `docs/LECTURE_ALIGNMENT.md`; page numbers below are 1-based PDF pages, not printed slide numbers:

| CGF topic | PDF and pages | Where to show implementation |
| --- | --- | --- |
| Shaders, attributes/uniforms | `L-2-2 Computer Graphics Fundamentals.pdf`, 2–3, 7–9 | `Program`, `Mesh`, `Renderer::Impl::draw`, `table.vert/table.frag`. |
| Model/view/projection and normal matrix | `L-4-1_Transformations.pdf`, 23, 45, 48 | `transform`, `draw`, `render`; formula `projection * view * model * position`. |
| Camera and perspective | `L-5-1_Viewing.pdf`, 3, 11–12, 43, 46 | `Renderer::Impl::render`: `glm::lookAt`, `glm::perspective`, framing mapped to the full framebuffer. |
| Lighting | `L-5-2_Shading.pdf`, 14, 29, 42, 58, 66–70 | `table.frag`: normalization, Blinn–Phong/halfway vector, local lamps and candles. |
| Textures and UV | `L-8_TextureMapping.pdf`, 24–25, 43–47 | `ArtTexture::load`, `Textures`, `makeTopPlane`, sampler/UV scale in the main shader. |
| Input, window, resize | `L-3-1_Input.pdf`, 14–15, 24, 39 | `Window`, main `main` loop, `resetCamera`, `Renderer::render(width,height)`. |

`docs/COURSE_REQUIREMENTS.md` contains demonstration/report requirements. This is modern OpenGL Core; do not bring `glMatrixMode`, GLUT, `gl_FragColor`, or `texture2D` from historical slides into the existing pipeline. ImGui is an overlay, not a replacement for the custom 3D scene.

Remaining handoff gaps: no editable source assets or art-generation manifest; no separate public license selected for the project's own code/illustrations; no built-in run save or audio stack/custom music package. Linux/macOS support is present in code branches, but do not claim a particular delivery is portable based only on those branches. Lecture PDFs retain their authorship and are not part of the runtime package.
