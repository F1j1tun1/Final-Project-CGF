# Jester Jack architecture

Jester Jack is an educational blackjack prototype with a 3D table, endless rounds, and a relic shop. See [README.md](../README.md) for build and launch instructions.

## Technology stack and structure

| Area | Technologies |
| --- | --- |
| Application | C++17, GLFW 3.4, Dear ImGui 1.91.9b |
| Graphics | OpenGL 3.3 Core, GLSL 330, GLAD, GLM 1.0.1 |
| Build and tests | CMake 3.24+, CTest, desktop/core-only presets |

Dependencies are included in `third_party/`; CMake does not download libraries. The executable and target are named `blackjack_forge`.

| Files | Purpose |
| --- | --- |
| `src/game/` | Rules, deck, outcomes, rewards, run state, and shop; no graphics, files, or clocks |
| `src/presentation/` | Queue for showing cards, revealing the dealer, and awarding rewards; no game commands or random numbers |
| `src/graphics/BandModel.*`, `src/presentation/BandSync.*` | Stage jazz ensemble and synchronization to soundtrack beats (details below) |
| `src/audio/` | Audio: `Mixer` decides what plays and when (no OS or devices), while `Engine` plays on a separate thread; no rules or graphics |
| `src/graphics/` | Meshes, textures, shaders, camera, lighting, and GPU resources; no outcome calculations |
| `src/graphics/RendererRoom.inl` | Theater arches, curtains, columns, and room lighting |
| `src/main.cpp` | Window, input, menus, UI, transition timing, and conversion from Snapshot to Scene |
| `assets/` | Shaders, TGA illustrations, fonts, and their licenses |
| `assets/audio/` | Effects (`sfx/`), UI sounds (`ui/`), main theme, and soundtracks (`music/`); see `assets/audio/README.md` |
| `tests/game_tests.cpp` | Tests for rules, every relic, starting decks, revival, shop, and reproducibility |
| `tests/presentation_tests.cpp` | Tests for display order, acceleration, active abilities, and independence from time-step size |
| `tests/band_tests.cpp` | Tests for the ensemble and parsing synchronization files |
| `tests/audio_tests.cpp` | Tests for soundtrack shuffling, main-theme looping, overlapping effects, ducking, and missing files (with a fake backend and no sound card) |
| `src/presentation/EntranceTransition.hpp` / `tests/entrance_tests.cpp` | Frame-rate-independent entrance stages, skipping, short fade, and restart |
| `scripts/balance_sim.cpp` | Reproducible progression comparison without access to hidden cards |

## Rules and state

A hand is one play against the dealer; a round has a shared target and five hands, or six with LAST LAUGH. Targets are 30, 38, 46, 54, 62, 70, 78, and then 86 indefinitely. After meeting the target, continuing opens the shop. If the target is not met by the final hand, the game automatically enters `RunLost`; meeting the target takes priority. The next round restores the full hand allotment and resets the round score while preserving coins and relics. Unused hands do not carry over. There are no bets or final victory.

The dealer stands on 17, including soft 17. An ace counts as 1 or 11. A natural blackjack is 21 on the first two cards and beats an ordinary 21. A winning hand awards points equal to its total and 4 coins, plus relic effects; a natural blackjack without relics awards 21 points. A push awards zero and preserves the streak; a lost hand awards zero and breaks the streak. Both use a hand but do not by themselves end the run.

`Game` accepts the commands `deal/hit/stand/advance/buy/sell/replace/activate/chooseCard/leaveShop/revive` and provides a `Snapshot`. An invalid game command returns `false` without changing state or the future random-number sequence. `restart(seed, deck)` starts a new run with the selected deck; `restart(seed)` preserves the deck selection. State includes the phase, cards, round score and target, hands remaining, total points and wins, coins, streak, relics, and shop offers. `startDeck/maxHands/revivesLeft/revivalsUsed` describe the starting deck and its resources. `owned` is the sole equipment list, limited by `maxEquipmentSlots = 8`; no separate decorative inventory is created. `usedHandAbilities/usedRoundAbilities`, `cardChoices`, `pendingRescue`, and `alteredThisHand/rescuedThisHand` track charge use, unfinished choices, and joint-effect conditions. `abilityStatus` reports remaining charges, availability, and the reason an ability is unavailable.

Each hand shuffles a new 52-card deck uniformly with Fisher–Yates and rejection sampling. Separate `std::mt19937` generators for cards and the shop preserve reproducibility: the same seed, deck, and actions produce the same result. Purchases do not affect the card stream; the natural-blackjack rate is not artificially adjusted. The application chooses a new seed on a normal launch; `--seed N` fixes it, and `--smoke` uses 42 by default.

## Starting decks and revival

After pressing START, the player chooses one run advantage:

| Menu name | `StartDeck` | Effect |
| --- | --- | --- |
| LAST LAUGH | `ExtraHand` | Six hands per round |
| ENCORE | `SecondLife` | One revival per run, five hands per round |
| GOLDEN FOOL | `Rich` | 10 starting coins, five hands per round |

`Standard` is the internal default for the API and launches without a selection: five hands with no extra advantage. It is not offered in the menu.

For ENCORE, a REVIVE button appears in `RunLost` while a life remains. `revive()` retries the same round with the same target: it resets the round score and streak, restores the hands, Second Chance charge, and active-ability charges. Coins, relics, total points, and win count are preserved. Revival is used once and does not change the seed or rewind the random generators: subsequent cards continue from the current random stream. The player can start a new run instead.

## Relics and the shop

The catalog contains 40 relics: 14 Common, 15 Rare, and 11 Legendary. All point, multiplier, and coin bonuses apply only on a win; Second Chance protection and the four active abilities work before the outcome is determined.

The final score is `lround((base + bonus) * multiplier)`, where `base` is the winning hand total, `bonus` is the sum of flat point bonuses, and `multiplier = 1 + sum of multiplier bonuses`. For example, +0.5 and +1.0 together give x2.5. Coins are not multiplied.

The full [40-relic catalog](catalog-balance.md) lists prices, conditions, and limits. Active Rewind and Redraw alter only the last drawn card; Forked Path offers a choice between two cards; Lifeline removes a busting card. The first three have one charge per hand, while Lifeline has one per round. A bust waits for a decision only when an applicable rescue is available; until then, the dealer remains hidden and no reward is awarded. STAND accepts the loss, and a completed hand cannot be reopened. Alchemist strengthens a win after an active ability; Phoenix strengthens it after a genuine bust rescue in the same hand.

Second Chance cancels a card before a loss is determined, allowing the player to continue. The cancelled card is not returned to the deck. Its charge refreshes between rounds and on revival; selling and buying it again does not restore the charge. Its multiplier bonus applies only to a rescued hand if that hand ends in a win. The Encore relic and the ENCORE starting deck have different effects.

The shop offers two different unequipped relics for the entire visit. Each slot first selects an available rarity by Common/Rare/Legendary weights of 50/36/14, then selects a relic uniformly within that rarity. Empty rarities are excluded and the weights are renormalized; the item already selected for the first slot is excluded from the second. Within the selected rarity, priority goes to relics that were not offered on the previous visit; if none are available, a relic of the same rarity may repeat. This preserves the available rarities, so the weights are not fixed odds for any individual relic.

The player can hold up to eight distinct relics, buy one or both offers, or buy neither. Prices by rarity are 3/6/10 coins; selling returns half the price rounded down, or 1/3/5 coins. A sold relic may appear later. Purchases and sales do not refresh the current pair, and an item bought and sold during the same visit cannot be bought again. `replace(equipped, offered)` checks all conditions and the balance after accounting for the sale of the old relic, then replaces one slot without an intermediate sale. A failed replacement changes nothing. Run saves are not implemented.

`ScoreBreakdown` stores the base, total bonus, total multiplier, final points, coins earned, and `appliedEffects`. Each `AppliedEffect` entry records the `relic`, `points`, `coins`, and `multiplierBonus` that actually applied. Calculating the score and recording each contribution happen together; the UI uses the resulting data. Effects follow catalog order; the list is empty on a loss or push.

## Presentation sequence

`Game` resolves a command immediately, while `presentation::Presentation` stores an intermediate visible snapshot and a queue of visual steps. Cards appear one at a time; the dealer reveals the hidden card and draws, then a winning reward shows the base, each applied effect, and the total. New game commands are blocked while the queue is active. A click or Space completes the current step; speed is set in OPTIONS. Presentation does not change rules or random-number generators. `Action::Ability` shows a card being removed, replaced, or added while synchronizing charge and choice metadata; the dealer's hidden card remains hidden until the actual outcome.

At normal speed, a card takes 0.55 s, the flip and reward base take 0.4 s, a rescue and each individual effect take 0.5 s, and the total takes 0.6 s. The slider shortens each step to 0.07 s. The queue carries elapsed time across steps, preserving the result at different frame rates. Legendary purchases, rescues, and Legendary effects each receive a separate highlight; awarded points remain visible after the animation.

## Audio

Audio does not affect the rules or outcomes. `Presentation` marks event times with `Cue` events (bet, initial deal card, player card, dealer card, hand win, round target reached, coins, hand loss, run end) exactly when the corresponding visual step begins; `main.cpp` retrieves them through `takeCues()` and plays the appropriate effect. This keeps sound aligned with animation, speeds it up with the animation, and prevents replay when a step is skipped.

`audio::Engine` runs on its own thread and accepts queued commands; opening an MP3 and waiting for the device do not block a frame. On Windows, MP3 playback uses the built-in MCI decoder (`winmm`), with no extra libraries; on other systems and in hidden runs (`--hidden`, `--smoke`, captures), `Engine` plays nothing. `audio::Mixer` is OS-independent: it keeps two voices per effect (allowing quick repeats to overlap), tracks when a soundtrack ends, and starts the next one. Soundtracks from `music/soundtrack_*.mp3` play in shuffled cycles: each cycle plays every track once and does not repeat a track consecutively. The main theme loops on every menu screen; it switches to soundtracks when TAKE A SEAT is pressed, and restarts when returning to the menu. Music is ducked on the game-over screen.

| Event | Sound |
| --- | --- |
| Deal (DEAL HAND) | `sfx/chips_bet.mp3` once; then `sfx/card_deal.mp3` ("Dealer's distribution of cards") for each of the four cards dealt to the player and dealer |
| HIT, Redraw, Forked Path card selection | `sfx/card_roll.mp3` ("The player's card roll") |
| Dealer draws | The same `sfx/card_roll.mp3` |
| Hand win | `sfx/tada_minor.mp3`, then `sfx/chips_win.mp3` when the reward total appears |
| Win that completes the round target | `sfx/tada_major.mp3` instead of minor, then `sfx/chips_win.mp3` |
| Hand loss | `sfx/chips_win.mp3`: the dealer takes the bet |
| Run ends | `sfx/you_lose.mp3` |
| New run starts | `sfx/chips_start.mp3` |
| Purchase / replace a relic | `sfx/chips_bet.mp3`; sale: `sfx/chips_win.mp3` |
| Press a button, checkbox, collection item, F1 (open) | `ui/ui_open.mp3` |
| BACK, DONE, GOT IT, MENU, Escape, F1 (close) | `ui/ui_close.mp3` |
| Cursor enters a button (menu only, not during a run) | `ui/ui_selection.mp3` |
| Car drives along the main-menu street | `ambient/car_pass.mp3`, see below |
| Rejected command or click on an unavailable offer | `ui/ui_error.mp3` |

There are no bets in the game, so “bet” refers to a hand: it is the hand that uses one of the round's available hands.

### Main-menu car

The sedan drives along the menu street every 32 seconds (`Renderer::drawMenuSedan`: movement depends only on scene time). The `ambient/car_pass.mp3` recording (left-to-right pass, peak volume around 5.1 s, with stereo panning crossing center) starts at the beginning of each cycle at offset `phase + 0.45 s`. This aligns “car opposite the camera” with the loudest moment and the pan crossing center. If the menu opens mid-cycle, playback starts at the corresponding offset. On TAKE A SEAT or when Ambient motion is disabled (the car is not rendered), the sound fades out over 1.2 s. The recording was normalized (+6 dB with a limiter); the original was not changed.

## Jazz ensemble

Five musicians (pianist, electric guitarist, drummer, trumpeter, and saxophonist) stand on the stage behind the dealer and play along with the current soundtrack. They wear black suits and white shirts; the guitarist, trumpeter, and saxophonist wear hats.

| File | Role |
| --- | --- |
| `src/graphics/BandModel.{hpp,cpp}` | Geometry and poses: a flat list of primitives (cube, sphere, cylinder) with a matrix and material. No OpenGL, making it deterministic and testable. |
| `src/presentation/BandSync.{hpp,cpp}` | Beat grid and soundtrack volume curves, loaded from `assets/audio/music/soundtrack_N.sync` |
| `scripts/analyze_soundtrack.py` | Creates `.sync` files: beat tracking (dynamic programming, Ellis, 2007), downbeats, low/mid/high-frequency energy, and attacks sampled four times per second |
| `scripts/band_preview.cpp` | Software rasterizer for previewing poses without a GPU (developer tool) |
| `tests/band_tests.cpp` | Final matrices, scene bounds, determinism, frame-to-frame smoothness (no jumps), volume response, and `.sync` parsing |

![Jazz ensemble preview](images/jazz-band-preview.png)

*Preview generated by `scripts/band_preview.cpp` (software rasterizer without the game's lighting).*

The model uses meters (a person is about 1.8 m tall) and is placed on the stage floor at scale 1.7 so all five musicians read clearly from the table camera and do not extend behind the curtains.

How the musicians play:

- **Timing.** `Engine::musicClock()` returns the track index and position (from MCI `status position`, with interpolation between samples); `SoundtrackSync::beatAt()` converts this into a continuous beat count. Every movement follows that count: swung eighth notes (the second eighth at two-thirds of a beat), downbeats, and chord changes every two bars. Above 140 BPM, they play on every other beat.
- **Dynamics.** Each musician's effort comes from the volume curves: the drummer follows attacks and low frequencies, trumpet and saxophone follow high and mid frequencies, and piano and guitar follow mid frequencies. In quiet passages and between tracks they play more softly and relax; the trumpet lowers.
- **Anatomy.** Arms and legs use two-bone IK (shoulder, elbow, wrist). The pianist plays 52 white and 36 black keys with the right hand (melody) and left hand (chords), and uses a pedal. The guitarist has a strap, fretted neck, and strings; the left hand moves along the neck between chords, the right hand strums, and one foot keeps the beat. The drummer plays a swung ride pattern, snare, pedal hi-hat, toms, and bass drum with beater; fills occur every four bars, and the sticks arc toward cymbals and drums. The trumpeter presses three valves according to fingering, lifts the bell with the phrase, and puffs the cheeks. The saxophonist has a strap and keys, sways in time, and raises the bell on long phrases.
- **Without audio** (hidden runs or missing files), the ensemble plays a demonstration groove at 110 BPM so the stage does not go still.

Limitations: these are stylized mannequins made from primitives, like the rest of the scene, not a performance reconstructed from a recording. The ensemble knows the track's tempo, beats, and volume, but not individual notes, so its fingers play plausible figures rather than reproducing the recording. From the standard camera, the stage is distant and the musicians are about 45 pixels tall.

## Rendering and UI

The application converts the visible Presentation snapshot and animation progress into a Scene. `Scene::tableViewport` contains normalized left/top/width/height values measured from the framebuffer's upper-left corner. Perspective is calculated for the table area, then translated and scaled into the full frame: card positions remain fixed while the room continues behind the transparent UI. A subtle top and bottom gradient is drawn over the scene beneath game labels and buttons. The menu uses a standard full-frame perspective. The vertex shader computes `projection * view * model * position`; normals are transformed by `transpose(inverse(mat3(model)))`. The fragment shader combines UV texture sampling with ambient/diffuse/specular Blinn–Phong lighting; the depth test determines surface visibility.

The casino, table, and card fronts are generated procedurally. The mask, clothing, gloves, and curtain folds use continuous surfaces with normals; static meshes load when the renderer is created. The dealer wears a purple-burgundy coat with muted gold trim, lace, and a porcelain mask. His sleeves update vertices and normals in a dedicated buffer; other parts and card motion change through model matrices. Short reactions to cards, wins, and busts follow the visible hand state. The checkerboard floor is computed in a single surface shader instead of using separate tiles.

The central arch frames the dealer; the next portals recede into the distance. Paired burgundy and purple curtains, rails, separate columns, and warm candles form a symmetrical theatrical composition without walls or ceiling. The tiled floor fades into natural blackness in the distance. One hanging lamp lights the table and enlarged dealer; its cable continues to an upper mount outside permitted camera angles. Dust particles are distributed through a world-space volume along three independent axes, drift slowly, respond to local lighting, and are depth-sorted for transparency. The central support of the enlarged table remains. Ambient motion stops decorative cycles. There are no extra tables, background characters, or objects scattered on the floor.

`ArtTexture` loads uncompressed 24/32-bit TGA files, accounting for BGR(A) order and image orientation. The UI uses an atlas of 24 original relic illustrations, procedural symbols for 16 new items, and three separate card backs; the renderer uses those backs for the stack and face-down cards too. The loader restores OpenGL unpack state; card backs are flipped vertically on 3D planes. GPU resources belong to RAII objects and are destroyed while the context is alive. ImGui is drawn over the 3D scene. The `assets` folder is copied next to the executable; `--assets DIR` can set a different asset root.

In the menu, an enlarged title with jester ornament and the START/OPTIONS/COLLECTION/EXIT buttons sit on the left; the casino palace is on the right in subdued evening light. A dark sedan with headlights periodically drives along the road; its movement uses scene time and not game randomness. START opens a carousel of three decks with card backs and advantage descriptions; selection uses on-screen arrows and the left/right keys. After TAKE A SEAT, the buttons fade, the camera approaches the door and slows down; the doors open during deceleration, then a short move inside ends in a fade. The complete transition takes 4.6 seconds; the scene changes once during a fully black frame. A second start during the transition is rejected; returning to the menu resets the transition. Escape skips the entrance, while disabling Cinematic entrance uses a 0.6-second fade without camera movement. COLLECTION shows all 40 relics, rarity filters, and a separate description area for the selected item.

During gameplay, the target and counters are on the left, and the enlarged table and dealer-jester are centered on the right. Physical chips on the table match `owned`: zero at the start of a run, up to eight, with no decorative duplicates. Hovering shows the name, rarity, and effect; clicking a chip or pressing 1–8 selects a relic for detailed inspection. Inspection does not use a charge. Only the separate A key or ACTIVATE button uses a selected available ability; its charge and the reason it is unavailable are shown nearby. Purchases, sales, replacement, and restart immediately synchronize physical chips with equipment. During each AppliedEffect step, the corresponding chip rises and glows with its rarity color, and its actual point, coin, or multiplier bonus appears beneath it. HIT/STAND are centered at the bottom; the hand total is at the bottom right, with a bust shown in red and awarded points separately. After HIT, a sign/delta/suit hint lasts about 1.5 seconds and accounts for ace recalculation. Before the reveal, the dealer's visible total is shown with `+ ?`. Space performs the action for the current phase or speeds up the current animation; Enter activates the highlighted button. HOW TO PLAY contains the full rules.

GAME OVER smoothly darkens the scene and shows a white heading, reached round, final score, and three transparent buttons with white borders. Red fill appears only on hover or mouse press; keyboard focus is shown with a white border. The first button uses the remaining ENCORE life if available, otherwise starts a new run; the other buttons return to the menu or exit.

On an empty area of the table, holding the right mouse button rotates the camera; the scroll wheel changes distance; double right-click restores the default view. Yaw and zoom are limited; the menu, shop, Game Over, and open panels ignore camera gestures. Settings also include RESET VIEW.

Cormorant Garamond and Source Sans 3 fonts are distributed under SIL OFL 1.1. Component sources and licenses are listed in [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md); course techniques are mapped to lectures in [LECTURE_ALIGNMENT.md](LECTURE_ALIGNMENT.md).
