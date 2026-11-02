# Jester Jack — gameplay loop and UI

Current context as of October 22, 2026. This is a single-player blackjack game with relics, endless rounds, and a theatrical casino. The game UI is in English. If older discussions or documentation conflict with the implementation, the current [Game.hpp](../../src/game/Game.hpp), [Game.cpp](../../src/game/Game.cpp), [Presentation.cpp](../../src/presentation/Presentation.cpp), and [main.cpp](../../src/main.cpp) are the source of truth. This file describes project state; it does not introduce new rules.

## Rules and progression

- There are no bets. The first-round target is 30 points; each following round adds 10, and from round eight onward the target stays at 100. There is no final winning round.
- Usually a round has five hands. LAST LAUGH gives six. A hand is used when DEAL HAND is dealt; HIT does not use another hand.
- An ace counts as 1 or 11; J/Q/K count as 10. The dealer draws to 17 and also stands on soft 17. A natural blackjack is exactly two cards totaling 21 and beats an ordinary 21. Two natural blackjacks push.
- HIT draws a card; reaching an ordinary 21 automatically stands. A bust loses the hand unless Second Chance is unused. The actual bust total is displayed, without capping it at 21.
- A win awards points and coins. A lost hand and a push award zero but do not by themselves end the run. A loss resets the win streak; a push preserves it. Moving to the next round also preserves the streak.
- After a hand ends, CONTINUE either returns to the next hand or opens the shop if the target has been met. Remaining hands and points above the target do not carry to the next round.
- If the target is not met after the final hand, the logic immediately enters RunLost; no extra CONTINUE press is needed. Presentation first finishes the cards/flip, then opens GAME OVER. Meeting the target on the last hand takes priority over running out of hands.
- The shop opens only after a round is completed, not between individual hands. NEXT ROUND advances the round number, resets its score, restores the hands and Second Chance. Coins, relics, total score, and streak are preserved.

Code phases: `Ready → PlayerTurn → RoundOver → Ready/Shop`; a failed run transitions to `RunLost`. The name `finishRound()` is historical: the method resolves one hand, not an entire round.

## Starting decks

START opens a carousel with one large card back, arrows, and a description. The selected card back is used for the deck stack and face-down cards throughout the run.

| UI name | ID | Effect |
| --- | --- | --- |
| LAST LAUGH | `ExtraHand` | Six hands per round instead of five; no starting coins or revives. Selected by default in the menu. |
| ENCORE | `SecondLife` | Five hands; one voluntary revival for the entire run. |
| GOLDEN FOOL | `Rich` | Five hands; +10 coins at the start of the run. |

`StartDeck::Standard` is a technical option with five hands and no advantage, used by the Game constructor by default. It is not in the menu.

After a loss, ENCORE displays REVIVE instead of RETRY. Revival repeats **the same round number and target**, with zero round points, a full hand allotment, and Second Chance available again. Coins, relics, accumulated total score, and win count are preserved; the streak resets. The life is used once. Card generators are not rewound: this is a new attempt, not a replay of the same cards. Regular RETRY starts a new run with the selected deck and clears accumulated progress.

## Points, money, and the shop

For a winning hand, the base points equal its total; base income is 3 coins. A natural blackjack without relics awards **21 points**; there is no automatic +10 bonus.

```text
multiplier = 1 + the sum of applied multiplier bonuses
points = lround((hand total + added points) × multiplier)
coins = 3 + the sum of applied coin bonuses
```

Multiplier bonuses add together rather than multiply each other. For example, a natural blackjack with Blackjack Bonus, Precision, and Jester Crown awards `(21 + 5) × (1 + 0.5 + 1.0) = 65` points and 4 coins. Coins are not multiplied. All reward conditions are checked only on a win; the Second Chance defense can activate before the outcome.

At most four different relics are equipped at once. Purchases are only available in the current shop, with enough coins, and from its offers. The player may buy both offers or leave without buying. Sales are only available in the shop and return half the price rounded down: 1/3/5 coins. A sold relic may appear in a future shop; an item bought and then sold during the same visit cannot be bought again. Selling/buying does not recharge Second Chance within a round.

| Rarity | Count | Purchase | Sale | Rarity selection weight |
| --- | ---: | ---: | ---: | ---: |
| Common | 10 | 3 | 1 | 55 |
| Rare | 8 | 6 | 3 | 35 |
| Legendary | 6 | 10 | 5 | 10 |

For each of the two slots, the shop first selects a rarity by weight **among available rarities**, then selects an item uniformly within it. Equipped relics and the first offer are excluded. Within the selected rarity, items not offered in the previous shop are preferred; if none are available, an old offer may repeat. Duplicate protection does not remove an entire available rarity or suppress Legendary. Therefore 55/35/10 are base rarity weights, not fixed odds for any individual item; if a rarity is unavailable, the remaining weights are normalized. Offers do not change after buying or selling during a visit.

## Full catalog: 24 relics

Order follows the enum and catalog and also maps to illustration cells. “Streak before the hand” means the number of consecutive previous wins; a push preserves it.

| ID / name | Rarity | Price | Effect |
| --- | --- | ---: | --- |
| `Precision` / Precision | Rare | 6 | Win with 21: +0.5 multiplier and +1 coin. Works for both natural and ordinary 21. |
| `LongHand` / Long Hand | Rare | 6 | Win with four or more cards: +0.6 multiplier and +1 coin. |
| `SecondChance` / Second Chance | Legendary | 10 | Once per round, remove the just-drawn card that caused a bust and keep the player's turn active. Win that rescued hand: +1.0 multiplier. |
| `Streak` / Streak | Rare | 6 | +0.1 multiplier for each prior win in the streak, up to +0.4. Every third streak win (3, 6, 9…) awards +1 coin. |
| `RoyalPurse` / Royal Purse | Common | 3 | Win with a natural blackjack: +1 coin. |
| `SoftTouch` / Soft Touch | Common | 3 | Win with a soft hand: +1 coin. |
| `NarrowEscape` / Narrow Escape | Rare | 6 | Win when the dealer busts: +0.4 multiplier and +1 coin. |
| `RedRibbon` / Red Ribbon | Common | 3 | Win with two red initial cards: +1 coin. Later cards are not checked. |
| `BlackVelvet` / Black Velvet | Common | 3 | Win with two black initial cards: +1 coin. Later cards are not checked. |
| `EvenOdds` / Even Odds | Common | 3 | Win with an even hand total: +1 coin. |
| `CourtFavor` / Court Favor | Common | 3 | Win with at least one J/Q/K: +3 points. |
| `HumbleCrown` / Humble Crown | Common | 3 | Win with a total of 18 or lower: +5 points. |
| `BlackjackBonus` / Blackjack Bonus | Common | 3 | Win with a natural blackjack: +5 points, no extra coins. |
| `CopperBell` / Copper Bell | Common | 3 | Win with three or more cards: +3 points. |
| `SeventeenCharm` / Seventeen Charm | Common | 3 | Win with exactly 17: +1 coin. |
| `ScarletMask` / Scarlet Mask | Rare | 6 | Win with an all-red hand: +0.5 multiplier. |
| `MidnightMask` / Midnight Mask | Rare | 6 | Win with an all-black hand: +0.5 multiplier. |
| `RoyalCourt` / Royal Court | Rare | 6 | Win with two or more J/Q/K cards: +0.75 multiplier. |
| `SoftCrown` / Soft Crown | Rare | 6 | Win with soft 19, 20, or 21: +0.75 multiplier. |
| `JesterCrown` / Jester Crown | Legendary | 10 | Win with a natural blackjack: +1.0 multiplier. |
| `FiveCardParade` / Five Card Parade | Legendary | 10 | Win with five or more cards: +1.5 multiplier. |
| `FinalAct` / Final Act | Legendary | 10 | Win on the round's last available hand: +1.0 multiplier. Counts six hands for LAST LAUGH. |
| `Underdog` / Underdog | Legendary | 10 | Win with 16 or lower when the dealer busts: +1.5 multiplier. |
| `Encore` / Encore | Legendary | 10 | Win after at least three prior consecutive wins: +1.0 multiplier. This relic is separate from the ENCORE starting deck. |

## Fair randomness

Each hand starts with a new, uniformly shuffled standard 52-card deck; cards are drawn without replacement within the hand. A card discarded by Second Chance is not returned. The implementation uses Fisher–Yates with rejection sampling, without changing blackjack odds, selecting “helpful” cards, or depending on the score or money.

Cards and the shop use separate `mt19937` generators; purchases, sales, and decorative animation do not consume the card generator. The same seed, deck, and sequence of valid commands produce the same result; blocked commands do not change state or future randomness. A normal launch gets a seed from `std::random_device`; `--seed` enables reproducible launches. A new run from the UI gets a fresh seed, or the next seed when an explicit fixed seed is used. The 55/35/10 weights apply only to the shop.

## Screens, presentation, and controls

Main menu: large Jester Jack title and large buttons on the left, casino on the right. START is green, OPTIONS gray, COLLECTION purple, EXIT red. The title reads normally from left to right. START → select one of three decks → TAKE A SEAT → fade/fade-in of the table over 0.9 s → DEAL HAND. COLLECTION contains all 24 items, rarity filters, and a separate area for a detailed description. MENU returns to the main menu; there is no separate saved-run continue button.

At the table, the target, round score, hands, coins, and reward are on the left; four relic slots are across the top without a shared opaque bar. The chip is on the left of its own slot, with the **name, rarity, and large description always visible** beside it. In the top gameplay row, tooltips contain only purchase/sale prices and do not repeat the effect. Hover or keyboard focus slightly raises and tilts a chip. Common is a light neutral, Rare blue, and Legendary gold. The shop shows two large illustrations with text, clear rarity labels, a large balance with a coin icon, and four sale slots.

The hand total is large at the lower right; soft hands and busts are labelled, with busts in red. After HIT, the total change and suit appear for about 1.5 s, accounting for ace recalculation. Before reveal, the dealer's total shows the up-card with `+ ?`. HIT/STAND are centered at the bottom.

Game resolves an action immediately; Presentation displays cards one at a time, the reveal, dealer draw, then the reward base, each applied effect, and the total. `ScoreBreakdown::appliedEffects` contains relic IDs and actual point/coin/multiplier bonuses; the UI uses those values instead of recalculating the balance. Order follows the catalog. On a loss or push, the list is empty. When an effect activates, its chip rises, glows, and shows its bonus. New game commands are blocked while the queue is active; acceleration completes the current step without changing the outcome.

Normal step durations: card 0.55 s, flip/base 0.4 s, rescue/effect 0.5 s, final 0.6 s. OPTIONS can shorten them to 0.07 s. Settings and rules pause presentation. Ambient motion controls decorative cycles separately from the deal.

GAME OVER smoothly darkens the scene and shows a white heading, reached round, total score, and three transparent buttons with white borders: REVIVE/RETRY, MAIN MENU, QUIT GAME. Red fill appears only on hover/press; keyboard focus uses a white border.

| Input | Action |
| --- | --- |
| Mouse | Buttons, item selection in COLLECTION, purchases, and sales. |
| ← / → in carousel | Previous/next starting deck. Confirm with TAKE A SEAT by mouse or Enter on the highlighted button. |
| H / S | HIT / STAND, if presentation and windows do not block the game. |
| Space | In the menu, open the carousel; in game, DEAL, STAND, CONTINUE, NEXT ROUND, or REVIVE/RETRY depending on the phase. Does not perform a second step at once or duplicate a highlighted-button activation. |
| Click / Space during presentation | Finish the current animation step; does not issue the next game command. |
| Tab / arrows / Enter | ImGui keyboard navigation and activation of the selected element. |
| F1 | Open/close OPTIONS. |
| Escape | First close HOW TO PLAY or OPTIONS, then COLLECTION/deck selection; without windows, return to the menu. A window behind settings must not close before settings. |
| Hold right mouse button on an empty table | Rotate camera. The left mouse button is not used for the camera. |
| Scroll wheel on an empty table | Zoom in/out. |
| Double right-click / RESET VIEW | Reset camera to `RenderSettings{}` while preserving other settings. |

The camera ignores gestures in the menu, shop, GAME OVER, and over open panels. Current defaults: yaw 0°, pitch 32°, distance 9.1; limits are yaw ±35°, pitch 32–68°, distance 8–14. Minimum window size is 1100×720; 1440×900 is also checked. OPTIONS contains Lighting, Textures, Ambient motion, Start transition, Light, Dealing speed, and RESET VIEW. There are no audio settings yet.

## Latest accepted visual decisions

Style: a calm, mysterious theatrical casino without horror or pixel text. Palette: warm ivory, dark ink shades, aged brass, burgundy/purple, and table green; saturated accents are used sparingly. Fonts: Cormorant Garamond and Source Sans 3. Relics are large collectible chips with unique illustrations; the carousel shows one large card back with a gentle ~0.2 s motion.

The main table is in the foreground, with a single dealer-jester in the center, a central arch behind him, and portals receding in perspective. Separate columns, warm candles, **symmetrical** burgundy inner and purple outer curtains of matching height within each pair, with aligned rails, rings, and ornaments. No walls or ceiling; the tiled floor and depth fade into natural darkness.

Two matching hanging lamps are raised above the dealer's head: lamp centers at `x=±4.7, y=3.1, z=−5.75`. One function synchronizes their materials and local lighting. Soft dips occur every 31 s: `1 − .16·exp(−((phase−12)/.55)²) − .07·exp(−((phase−13.3)/.45)²)`; with Ambient motion off, brightness is 1. No rapid blinking. Do not replace them with the old single low lamp.

The 3D scene is drawn across the full frame behind the UI; off-axis projection preserves the table's position within its area. A **subtle top/bottom gradient** is allowed for readability—140 px, black, maximum alpha 48/255. There must be no rectangular black crop around the scene. The menu shows the casino on the right and a dark sedan with headlights passing periodically; decorative motion is unrelated to game randomness.

| Current and accepted | Older rejected option |
| --- | --- |
| One main table and one dealer; arches, curtains, and columns provide depth. | Four mini-tables with small dealers, then two full-size distant tables with chairs. In the end, **all extra furniture and background NPCs were removed**. |
| Clean floor, main table legs, and standard items on the table surface. | Masks, cards, chips, bells, and symmetric card fans scattered across the floor. |
| Matching paired curtains and two tall lamps. | Asymmetrically gathered curtains and one low lamp. |
| Readable large descriptions beside each chip; gameplay-row tooltip shows prices only. | Small description hidden in a tooltip, or a duplicate large popup panel. |
| Subtle gradient over a continuing scene. | Completely black empty area/hard render boundary or a heavy dark mask. |
| Three rarities, 24 relics, conditional additive multipliers. | Catalogs of 12/13 items, five rarities 50/28/14/6/2, old prices 2/4/6/8, automatic +10 for natural blackjack. |
| Multiple hands to reach a target; run ends only when hands run out below the target. | Ending the run after the first lost hand; opening the shop after every individual hand. |

## Current implementation limits and related documents

Custom sounds and music were requested as future project work; the audio system is currently absent. Saving a run between launches and a separate continue screen are not implemented. OPTIONS has no resolution/fullscreen selector. Do not present these features as finished.

Two small discrepancies remain between older text and the current UI: README broadly includes “deck selection” in the Space action, although the carousel is confirmed with TAKE A SEAT; HOW TO PLAY still suggests hovering to read an effect, although the description is already always visible beside the chip. They do not change actual controls or balance.

Build and launch instructions: [README](../../README.md); overall structure: [ARCHITECTURE](../ARCHITECTURE.md); submission requirements and lecture alignment: [COURSE_REQUIREMENTS](../COURSE_REQUIREMENTS.md) and [LECTURE_ALIGNMENT](../LECTURE_ALIGNMENT.md). Rules and presentation tests are in `tests/game_tests.cpp` and `tests/presentation_tests.cpp`. This handoff does not replace actual build and verification results.
