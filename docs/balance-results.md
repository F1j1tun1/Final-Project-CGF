# Seeded progression comparison

Measured on 2026-10-29. The expansion makes longer runs substantially more attainable under a reproducible, conservative policy. It does not guarantee long survival: early unlucky hands remain, and only 23 of 15,000 expanded runs cleared the 30-round observation cap.

## Versions and experiment

- Baseline core: `eb6611bc681b600794db660810ae899597f8c873` (`codex/visual-polish`).
- Expanded core: `53a6aa05c07fe7804d78d618b68c27ad8df1592a`; later presentation-only commits do not change this comparison.
- 5,000 seeds (`1..5000`) for each menu deck: Hands, Life and Coins. Each configuration therefore contains 15,000 runs. Standard, which is a command-line/default deck, is not included in these menu-deck aggregates.
- Stop at run loss with no remaining revival, or after clearing round 30. Means are capped observed means, not estimates of unlimited survival. The simulator automatically uses the Life deck's legitimate revival.
- A round is **reached** after clearing the preceding round. A run that loses in round 1 has zero rounds cleared. Percentiles use the lower empirical order statistic.
- Both builds use the same `scripts/balance_sim.cpp`, compiled against their respective core sources. The baseline disables the expanded API with `BJ_BALANCE_ACTIVE=0`; the current CMake target defines it as `1`.
- The run uses public game commands only. It neither edits snapshots nor looks at the dealer's hidden card or future deck order. Same seeds pair the initial random streams; changed offers, active draws and different run lengths naturally make later trajectories diverge.

The ordinary play policy hits hard 11 or less; stands on hard 12 against dealer 4..6 and hard 13..16 against dealer 2..6; otherwise hits those totals; and stands on hard 17 or greater. It hits soft 17 or less and soft 18 against 9, 10 or Ace. These are fixed hit/stand rules for the game's available actions, not a claim of optimal play.

The shop buys the highest-utility affordable offered item, then repeats while possible. At capacity it replaces the weakest item only for at least 15% higher utility and sufficient cash plus the sale refund. Utility ties use the relic enum so equipment order cannot affect a tie. Legacy item utilities are identical in both versions; explicit new-item priorities are listed in the simulator. These scores are documented heuristic preferences, not measured expected values. The baseline sells then buys; the expanded build exercises atomic `Game::replace`.

The expanded active policy uses Lifeline, then Rewind, then Redraw to escape a pending bust when available. It uses Forked Path when the ordinary policy wants a hit, choosing the highest nonbusting revealed total, or the lower bust if neither is safe. It may redraw an unfavorable hard 12..16 when the ordinary policy still wants a hit and the last card has rank 6 or above. Opening cards, settled rewards and hidden cards are never changed by the simulator. Every charge and validity check comes from the game API.

## Main comparison

| Deck | Mean rounds cleared, before → after | Median, before → after | 90th percentile, before → after | Reach round 5, before → after | Reach round 8, before → after | Reach round 12, before → after |
|---|---:|---:|---:|---:|---:|---:|
| Hands | 2.006 → 3.038 | 2 → 2 | 4 → 7 | 18.14% → 29.70% | 0.96% → 11.02% | 0.00% → 3.58% |
| Life | 2.365 → 3.572 | 2 → 3 | 4 → 7 | 21.16% → 38.40% | 0.98% → 13.08% | 0.00% → 3.88% |
| Coins | 1.481 → 2.092 | 1 → 1 | 3 → 5 | 9.78% → 19.44% | 0.22% → 5.58% | 0.00% → 1.38% |

Observed mean length increases by approximately 51%, 51% and 41% respectively. Round-8 attainment increases markedly, while unchanged Hands/Coins medians show that the early game still carries substantial random risk.

| Expanded deck | Reach round 20 | Reach round 30 | Clear round 30 cap | Mean maximum equipped | Runs that equipped all 8 | Mean active uses per run |
|---|---:|---:|---:|---:|---:|---:|
| Hands | 1.12% | 0.40% | 0.34% | 3.646 | 1,073 / 5,000 | 1.388 |
| Life | 0.68% | 0.12% | 0.10% | 4.573 | 1,418 / 5,000 | 1.592 |
| Coins | 0.20% | 0.04% | 0.02% | 3.295 | 876 / 5,000 | 0.786 |

The total is 3,367 runs that equipped all eight slots. Aggregate improvement does not mean every seed improves: the expanded version clears more / the same / fewer rounds in 31.68% / 63.16% / 5.16% of paired Hands seeds, 41.24% / 52.76% / 6.00% of Life seeds, and 25.16% / 70.26% / 4.58% of Coins seeds. Different offered builds can disadvantage an individual seed.

## Controls and interpretation

| Policy / version | Hands mean cleared | Life mean cleared | Coins mean cleared |
|---|---:|---:|---:|
| Baseline with shop | 2.006 | 2.365 | 1.481 |
| Expanded with shop, active commands disabled | 2.682 | 3.207 | 1.905 |
| Expanded with shop and active commands | 3.038 | 3.572 | 2.092 |
| Baseline with no purchases | 1.672 | 1.931 | 1.210 |
| Expanded with no purchases | 1.844 | 2.151 | 1.336 |

The active-disabled control keeps the same shop preferences and can still buy active items, but never activates them; it accepts pending busts. It is a conservative ablation, not an optimized passive-only shopping policy. It still improves substantially, so the main result is not solely a smarter action policy. The no-purchase control exposes the gentler round-goal curve; most of the longer-run gain comes from the expanded build's combined catalog, capacity, income and active choices.

The tested changes are: capacity 4 → 8; catalog 24 → 40; tier weights 55/35/10 → 50/36/14; win income 3 → 4; and goals `min(100, 30 + 10 * (round - 1))` → `min(86, 30 + 8 * (round - 1))`. Costs remain 3/6/10 by rarity and the shop still offers two items. Full effects, charges and limits are documented in [catalog-balance.md](catalog-balance.md).

This is an engineering balance check, not human playtesting or an optimal-policy search. Results depend on the disclosed shop priorities and active policy. With 5,000 seeds per deck, even a proportion near 50% has roughly ±1.4 percentage points of binomial sampling uncertainty; rare long-run outcomes have few observations and should not be overinterpreted. No baseline run reaching round 12 means zero in this sample, not mathematical impossibility. Further design changes should rerun this comparison and be tested with human play.

## Reproduce and render a real eight-item fixture

The desktop CMake build includes the explicit benchmark target; it is not a timing-sensitive CTest test:

```powershell
cmake --build build/desktop --config Release --target balance_sim --parallel 2
.\build\desktop\Release\balance_sim.exe 5000 8 30 shop current-raw.csv
.\build\desktop\Release\balance_sim.exe 5000 8 30 no-active current-no-active-raw.csv
.\build\desktop\Release\balance_sim.exe 5000 8 30 no-shop current-no-shop-raw.csv
.\build\desktop\Release\balance_sim.exe --trace 26 Life > fixture8.txt
```

`--trace 26 Life` emits 60 plain public-API commands (`deal`, `hit`, `stand`, `advance`, `revive`, `buy ID`, `leave`) and stops in round 5, Ready, with eight actually purchased items. The trace includes the legitimate Life revival. Its acquisitions, in order, are Copper Bell, Black Velvet, Grand Finale, Narrow Escape, Mixed Company, Court Favor, Lifeline and Velvet Dividend. A UI replay may stop after any acquisition to inspect 0..8 item layouts. Other active traces can additionally emit `activate ID`, `choose INDEX`, or `replace OLD NEW`.

For the historical executable, extract `src/game/Game.cpp` and `Game.hpp` from the baseline commit into a temporary `baseline/game` directory. Compile those with the current simulator as a C++17 executable, include the temporary baseline directory, and define `BJ_BALANCE_ACTIVE=0`. Then run `balance_sim 5000 4 30 shop baseline-raw.csv`. This keeps the benchmark source fixed while changing only the core and intended capacity argument. No baseline source was modified in the repository.

## Verification evidence

Release builds of both standalone benchmark variants completed with MSVC 19.51.36260. Every main/control batch completed without an action rejection or run/shop guard failure. The baseline and expanded primary batches were rerun; each reproduced its entire 15,000-row CSV byte for byte. SHA-256 fingerprints:

| Artifact | SHA-256 |
|---|---|
| `baseline-raw.csv` | `87f856fab2b65997de92ce55e26b04755aee1bb5401dcec242415b3dab9d7806` |
| `current-raw.csv` | `2dbb1d2c57fa36e0826e2dbc8714ed116440ab7cfe38e179aa6d43ba211c8fea` |
| `current-no-active-raw.csv` | `594c47ebeea8130246e7480e6ff0e7297022a490352e3890eb57f0246650211b` |
| `baseline-no-shop-raw.csv` | `5013ad025d3f137af8fde82f6fa760049957885818fadfe7d013b421a55c9664` |
| `current-no-shop-raw.csv` | `4c1b48c00bf7040a15df6879ec88a0c65ecf492a40eb8121f50c1d8b35af3a00` |
| Expanded `src/game/Game.cpp` | `4dd0319d66626597f44eb1609a01d08a5db64d873d961d4375744a62294e3224` |
| Expanded `src/game/Game.hpp` | `61cb1818c5dbb47f8a6a2e24983183c242843431508e42896f768978246d4db1` |

The raw CSVs, summary CSVs, extracted baseline and independent build configurations are retained locally under `C:\Users\erkin\Documents\Codex\2026-10-26\task\balance-review`. The generated UI fixture is `C:\Users\erkin\Documents\Codex\2026-10-26\task\palace-review\fixture8.txt`.
