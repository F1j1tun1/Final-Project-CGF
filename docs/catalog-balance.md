# Jester Jack relic catalog and balance

The catalog contains 40 unique relics: 14 Common (white), 15 Rare (blue), and 11 Legendary (gold). A new run starts with no relics. `Snapshot::owned` is the sole equipment list; its limit is `bj::maxEquipmentSlots = 8`. Each equipped relic corresponds to exactly one physical chip on the table.

## Progression and shop

| Rule | Before update | After update |
| --- | --- | --- |
| Relic slots | 4 | 8 |
| Win income | 3 coins | 4 coins |
| Round target | 30 + 10 per round, capped at 100 | 30 + 8 per round, capped at 86 |
| Common / Rare / Legendary weights | 55 / 35 / 10 | 50 / 36 / 14 |
| Shop offers | 2 | 2 |
| Common / Rare / Legendary price | 3 / 6 / 10 coins | 3 / 6 / 10 coins |
| Sale return | floor(cost / 2) | floor(cost / 2) |

The first round target is 30 points. The cap applies from round eight onward, and the number of rounds is unlimited. A standard round has five hands; LAST LAUGH has six. ENCORE provides one revival for the entire run, and GOLDEN FOOL starts with 10 coins. The dealer stands on hard and soft 17. The rules for natural blackjack, pushes, losses, shuffling, and earlier relics are preserved. Multiplier bonuses add together instead of multiplying each other. Losses and pushes award nothing.

The shop first selects a rarity by the listed weights, then an available relic within that rarity. Equipped relics and the first offer are excluded. Within the selected rarity, relics not offered during the previous visit are preferred. Each rarity has enough items to preserve its weight even with eight relics equipped. Purchases and sales do not affect the card generator. A replacement first checks the offer, ownership, whether the item was already purchased during this visit, and the balance including the sale proceeds; it then replaces the existing slot in one operation. Invalid commands preserve state and randomness. An item that was bought and then sold cannot be bought again in the same shop visit.

## Active abilities and limits

Keys `1–8` and clicks on physical chips only select relics for inspection of their name, rarity, effect, and charges. Inspection does not use a charge. To activate a selected available ability, press `A` or the ACTIVATE button. Passive relics trigger automatically when their conditions are met.

| Ability | Charges | Condition | Result |
| --- | --- | --- | --- |
| Rewind | 1 per hand | Your turn; at least one card has been drawn | Remove the last drawn card, including a busting card |
| Redraw | 1 per hand | Your turn; at least one card has been drawn | Replace the last drawn card with the next card in the deck; the replacement may bust |
| Forked Path | 1 per hand | Your turn; no bust and no unfinished choice | Draw two cards, keep one, and discard the other |
| Lifeline | 1 per round | Bust before the outcome is determined | Remove the busting card |

Rewind and Redraw cannot change the initial two cards or manufacture a natural blackjack. Drawing to 21 starts the dealer's normal turn. A bust pauses the hand only if an equipped, charged, and applicable Rewind, Redraw, or Lifeline is available; the dealer's hidden card remains hidden and no points have been awarded yet. STAND explicitly accepts the loss. A bust after all applicable charges have been used resolves normally. The automatic Second Chance shield takes priority and still works once per round.

The Forked Path choice must be completed by selecting one of the two cards before any hit, stand, or other ability. Invalid choice indices, repeated selections, unequipped abilities, passive relics, and actions after the outcome is decided do nothing. The charge is consumed on activation, including when opening the two-card choice, not when its animation finishes. A new hand resets per-hand charges and joint-effect flags. A new round or ENCORE revival resets per-round charges. Restart clears equipment, card choices, charges, and rescue flags. The game has no save/load system, so saved inventory does not need migration.

Alchemist rewards only a win after an active ability was used in the same hand. Phoenix rewards only if the player actually avoided a bust in that hand through Second Chance, Rewind, Redraw, or Lifeline. Neither pays on a loss or push, and neither can reopen a hand whose outcome has already been settled.

## Full catalog

All point, multiplier, and coin bonuses in the table require a win. Active abilities and automatic Second Chance rescue take effect before the outcome is determined.

| Relic | Rarity | Price | Effect |
| --- | --- | ---: | --- |
| Precision | Rare | 6 | Hand total 21: +0.5 multiplier and +1 coin |
| Long Hand | Rare | 6 | 4+ cards: +0.6 multiplier and +1 coin |
| Second Chance | Legendary | 10 | Cancel the first busting card once per round; win that same hand after rescue: +1.0 multiplier |
| Streak | Rare | 6 | +0.1 multiplier per previous win in the streak, up to +0.4; +1 coin for every third win |
| Royal Purse | Common | 3 | Natural blackjack: +1 coin |
| Soft Touch | Common | 3 | Soft hand: +1 coin |
| Narrow Escape | Rare | 6 | Dealer busts: +0.4 multiplier and +1 coin |
| Red Ribbon | Common | 3 | Both initial cards are red: +1 coin |
| Black Velvet | Common | 3 | Both initial cards are black: +1 coin |
| Even Odds | Common | 3 | Even hand total: +1 coin |
| Court Favor | Common | 3 | Hand contains a J, Q, or K: +3 points |
| Humble Crown | Common | 3 | Hand total is 18 or lower: +5 points |
| Blackjack Bonus | Common | 3 | Natural blackjack: +5 points |
| Copper Bell | Common | 3 | 3+ cards: +3 points |
| Seventeen Charm | Common | 3 | Hand total exactly 17: +1 coin |
| Scarlet Mask | Rare | 6 | All cards are red: +0.5 multiplier |
| Midnight Mask | Rare | 6 | All cards are black: +0.5 multiplier |
| Royal Court | Rare | 6 | 2+ J/Q/K cards: +0.75 multiplier |
| Soft Crown | Rare | 6 | Soft 19, 20, or 21: +0.75 multiplier |
| Jester Crown | Legendary | 10 | Natural blackjack: +1.0 multiplier |
| Five Card Parade | Legendary | 10 | 5+ cards: +1.5 multiplier |
| Final Act | Legendary | 10 | Last available hand of the round: +1.0 multiplier |
| Underdog | Legendary | 10 | Hand total 16 or lower and dealer busts: +1.5 multiplier |
| Encore | Legendary | 10 | Already on a streak of 3+ wins: +1.0 multiplier |
| Rewind | Rare | 6 | Active: remove the last drawn card, including a busting card; once per hand, cannot affect initial cards |
| Redraw | Rare | 6 | Active: replace the last drawn card with the next card in the deck; once per hand, replacement may also bust |
| Forked Path | Legendary | 10 | Active: draw two cards, choose one to keep, and discard the other; once per hand |
| Lifeline | Legendary | 10 | Active: remove a busting card before the outcome is decided; once per round |
| Velvet Dividend | Common | 3 | Every win: +2 coins |
| Lucky Seven | Common | 3 | Hand contains a seven: +7 points |
| Twin Sigil | Common | 3 | Initial cards have the same rank: +10 points |
| Mixed Company | Common | 3 | Hand contains both red and black cards: +5 points |
| Crescendo | Rare | 6 | 3+ cards: +0.75 multiplier |
| Ace High | Rare | 6 | Hand contains an ace: +0.75 multiplier and +1 coin |
| Silver Lining | Rare | 6 | Every win: +0.35 multiplier |
| Loaded Dice | Rare | 6 | Hand total is at least 18: +8 points |
| Alchemist | Rare | 6 | Win after using an active ability in the same hand: +0.8 multiplier and +2 coins |
| Phoenix | Legendary | 10 | Win after rescue from a bust in the same hand: +2.0 multiplier and +2 coins |
| Royal Decree | Legendary | 10 | Every win: +0.8 multiplier |
| Grand Finale | Legendary | 10 | After a winning hand, one or fewer hands remain in the round: +1.5 multiplier |

## Verification

Rules tests cover every equipment size from 0 to 8 through real shops and reproducible hands: capacity limits, replacing a full slot in one operation and rejecting replacement when coins are insufficient, purchase, sale, and restart. They verify ability availability and charges, both card-choice paths, deferred outcomes on a bust, reproducibility, and actual joint-effect rewards. Existing tests for starting-card distributions, shop weights, deck-card uniqueness, outcomes, rewards, the shield, and starting decks are preserved. The [balance comparison](balance-results.md) reports independent simulations before and after the update and explains the limitations of that estimate.
