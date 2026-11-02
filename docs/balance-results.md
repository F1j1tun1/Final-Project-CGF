# Progression comparison with fixed seeds

Measurements were run on 2026-10-29. With a reproducible, conservative strategy, the update substantially increases the chance of reaching longer runs. Long survival is not guaranteed: poor early hands still occur, and only 23 of 15,000 runs in the updated version passed the 30-round observation limit.

## Versions and methodology

- Baseline core: `eb6611bc681b600794db660810ae899597f8c873` (`codex/visual-polish`).
- Updated core: `53a6aa05c07fe7804d78d618b68c27ad8df1592a`; subsequent commits that change presentation only do not affect the comparison.
- For each menu deck—Hands, Life, and Coins—5,000 seeds (`1..5000`) were used, for 15,000 runs per configuration. Standard, used by default or from the command line, is not included in these summary results.
- A run ends on a loss with no revival remaining or after completing round 30. Averages are capped at this observation limit and do not estimate unlimited survival. The simulator automatically uses the Life revival provided by the rules.
- A round counts as **reached** after the previous round has been completed. If the player loses in round 1, the number of rounds completed is zero. Percentiles use the lower empirical order statistic.
- Both builds use the same `scripts/balance_sim.cpp` compiled against the corresponding core. The extended API is disabled in the baseline with `BJ_BALANCE_ACTIVE=0`; the current CMake target sets it to `1`.
- Only public game commands are used. The simulator does not edit snapshots, inspect the dealer's hidden card, or read the future deck order. Matching seeds align the initial random streams; shop changes, active-ability draws, and different run lengths naturally cause later trajectories to diverge.

The standard strategy hits on a hard total of 11 or less; stands on hard 12 against a dealer up-card of 4–6 and hard 13–16 against 2–6; hits on other totals in those ranges; and stands on hard 17 or higher. On soft 17 or lower it hits, and on soft 18 it hits against 9, 10, or an ace. These are fixed hit/stand rules for the available actions, not a claim of optimal play.

The shop selects the affordable item with the highest priority and repeats purchases while possible. With full slots, it replaces the weakest item only if the new item's priority is at least 15% higher and there are enough coins after accounting for the sale. Ties are broken by the relic enum value, so equipment order does not affect the result. Priorities for older items are identical in both versions; priorities for new items are listed explicitly in the simulator. These are heuristic preferences, not measured expected values. The baseline sells and then buys; the updated version uses atomic `Game::replace`.

When a bust awaits confirmation, the active strategy tries available Lifeline, Rewind, and Redraw in that order. Forked Path is used when the standard strategy calls for a hit: it chooses the highest displayed total that does not bust, or the smaller bust if no safe option exists. A failed hard total of 12–16 may be redrawn if the standard strategy still calls for a hit and the last card's rank is 6 or higher. The simulator never changes initial cards, awarded rewards, or hidden cards. The game API enforces charge and action validity.

## Main comparison

| Deck | Average rounds completed, before -> after | Median, before -> after | 90th percentile, before -> after | Reach round 5, before -> after | Reach round 8, before -> after | Reach round 12, before -> after |
|---|---:|---:|---:|---:|---:|---:|
| Hands | 2.006 -> 3.038 | 2 -> 2 | 4 -> 7 | 18.14% -> 29.70% | 0.96% -> 11.02% | 0.00% -> 3.58% |
| Life | 2.365 -> 3.572 | 2 -> 3 | 4 -> 7 | 21.16% -> 38.40% | 0.98% -> 13.08% | 0.00% -> 3.88% |
| Coins | 1.481 -> 2.092 | 1 -> 1 | 3 -> 5 | 9.78% -> 19.44% | 0.22% -> 5.58% | 0.00% -> 1.38% |

The observed average run length increased by approximately 51%, 51%, and 41%, respectively. The probability of reaching round 8 rose substantially, although the unchanged Hands and Coins medians show that significant random risk remains in the early game.

| Updated-version deck | Reach round 20 | Reach round 30 | Complete round 30 cap | Average maximum occupied slots | Runs that filled all 8 slots | Average active-ability uses per run |
|---|---:|---:|---:|---:|---:|---:|
| Hands | 1.12% | 0.40% | 0.34% | 3.646 | 1,073 / 5,000 | 1.388 |
| Life | 0.68% | 0.12% | 0.10% | 4.573 | 1,418 / 5,000 | 1.592 |
| Coins | 0.20% | 0.04% | 0.02% | 3.295 | 876 / 5,000 | 0.786 |

All eight slots were filled in 3,367 runs overall. Better aggregate results do not mean every seed improves: the updated version completed more / the same / fewer rounds in 31.68% / 63.16% / 5.16% of paired Hands runs, 41.24% / 52.76% / 6.00% of Life runs, and 25.16% / 70.26% / 4.58% of Coins runs. Different shop offers can produce a weaker build for an individual seed.

## Control variants and interpretation

| Strategy / version | Average Hands rounds completed | Average Life rounds completed | Average Coins rounds completed |
|---|---:|---:|---:|
| Baseline with shop | 2.006 | 2.365 | 1.481 |
| Updated with shop, active commands disabled | 2.682 | 3.207 | 1.905 |
| Updated with shop and active commands | 3.038 | 3.572 | 2.092 |
| Baseline without purchases | 1.672 | 1.931 | 1.210 |
| Updated without purchases | 1.844 | 2.151 | 1.336 |

The control with active commands disabled keeps the shop preferences and may buy active relics, but never uses them; busts awaiting confirmation are accepted. This is a conservative test that removes one capability, not an optimized strategy that buys only passive items. It also shows a substantial improvement, so the main result is not explained solely by smarter action selection. The no-purchase variant isolates the effect of smoother target progression; most of the run-length gain comes from the combined changes to the catalog, slot count, income, and active choices.

The changes checked were: slots 4 -> 8; catalog 24 -> 40; rarity weights 55/35/10 -> 50/36/14; win income 3 -> 4; targets `min(100, 30 + 10 * (round - 1))` -> `min(86, 30 + 8 * (round - 1))`. Rarity prices remain 3/6/10, and the shop still offers two items. Full effects, charges, and limits are described in [catalog-balance.md](catalog-balance.md).

This is an engineering balance check, not human playtesting or a search for an optimal strategy. Results depend on the disclosed shop priorities and active-ability rules. With 5,000 seeds per deck, even a proportion around 50% has roughly ±1.4 percentage points of binomial sampling uncertainty; rare long-run outcomes are based on few observations and should be interpreted carefully. No baseline runs reached round 12; that means zero in this sample, not mathematical impossibility. Repeat the comparison and add human playtesting after further design changes.

## Reproduction and showcase of a real eight-relic build

The desktop CMake build includes a separate balance-measurement target; it is not a time-dependent CTest test:

```powershell
cmake --build build/desktop --config Release --target balance_sim --parallel 2
.\build\desktop\Release\balance_sim.exe 5000 8 30 shop current-raw.csv
.\build\desktop\Release\balance_sim.exe 5000 8 30 no-active current-no-active-raw.csv
.\build\desktop\Release\balance_sim.exe 5000 8 30 no-shop current-no-shop-raw.csv
.\build\desktop\Release\balance_sim.exe --trace 26 Life > fixture8.txt
```

`--trace 26 Life` emits 60 ordinary public API commands (`deal`, `hit`, `stand`, `advance`, `revive`, `buy ID`, `leave`) and stops in round 5, in the Ready state, with eight genuinely purchased relics. The sequence includes the Life revival provided by the rules. Purchase order: Copper Bell, Black Velvet, Grand Finale, Narrow Escape, Mixed Company, Court Favor, Lifeline, and Velvet Dividend. Playback in the UI can be stopped after any purchase to inspect layouts with 0–8 relics. Other traces with active abilities may also emit `activate ID` and `choose INDEX`. `--trace 1 Life` creates an eight-relic build with Forked Path in slot 4; `--trace 17 Life` creates one with Redraw in slot 5 and Lifeline in slot 2. `fixturefork.txt` and `fixturerewind.txt` are saved next to `fixture8.txt` for checking the UI with real input.

For the historical executable, extract `src/game/Game.cpp` and `Game.hpp` from the baseline commit into a temporary `baseline/game` directory. Compile them with the current simulator as a C++17 executable, adding the temporary baseline directory to the header search paths and setting `BJ_BALANCE_ACTIVE=0`. Then run `balance_sim 5000 4 30 shop baseline-raw.csv`. This keeps the measurement tool source fixed while changing only the core and the capacity argument. Historical source files were not changed in the repository.

## Verification record

Release builds of both separate simulator variants were completed with MSVC 19.51.36260. All main and control runs completed without rejected expected actions or hitting the run/shop loop guards. The main baseline and updated runs were repeated; in both cases, the entire 15,000-row CSV reproduced byte for byte. SHA-256 checksums:

| Artifact | SHA-256 |
|---|---|
| `baseline-raw.csv` | `87f856fab2b65997de92ce55e26b04755aee1bb5401dcec242415b3dab9d7806` |
| `current-raw.csv` | `2dbb1d2c57fa36e0826e2dbc8714ed116440ab7cfe38e179aa6d43ba211c8fea` |
| `current-no-active-raw.csv` | `594c47ebeea8130246e7480e6ff0e7297022a490352e3890eb57f0246650211b` |
| `baseline-no-shop-raw.csv` | `5013ad025d3f137af8fde82f6fa760049957885818fadfe7d013b421a55c9664` |
| `current-no-shop-raw.csv` | `4c1b48c00bf7040a15df6879ec88a0c65ecf492a40eb8121f50c1d8b35af3a00` |
| Updated `src/game/Game.cpp` | `4dd0319d66626597f44eb1609a01d08a5db64d873d961d4375744a62294e3224` |
| Updated `src/game/Game.hpp` | `61cb1818c5dbb47f8a6a2e24983183c242843431508e42896f768978246d4db1` |

The source and summary CSV files, extracted historical version, and independent build configurations are saved locally in `C:\Users\erkin\Documents\Codex\2026-10-26\task\balance-review`. The sequence for demonstrating the interface is in `C:\Users\erkin\Documents\Codex\2026-10-26\task\palace-review\fixture8.txt`.
