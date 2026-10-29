# Jester Palace catalog and balance contract

The catalog contains 40 unique upgrades: 14 Common (white), 15 Rare (blue), and 11 Legendary (gold). A new run starts with zero equipped upgrades; `Snapshot::owned` is the sole equipment list, capped by `bj::maxEquipmentSlots = 8`. Each owned upgrade corresponds to one physical table chip.

## Progression and shop

| Rule | Previous | Current |
| --- | --- | --- |
| Equipment capacity | 4 | 8 |
| Win income | 3 coins | 4 coins |
| Round goal | 30 + 10 per round, cap 100 | 30 + 8 per round, cap 86 |
| Rarity weights | 55 / 35 / 10 | 50 / 36 / 14 |
| Shop offers | 2 | 2 |
| Common / Rare / Legendary cost | 3 / 6 / 10 coins | 3 / 6 / 10 coins |
| Sale credit | floor(cost / 2) | floor(cost / 2) |

Round 1 still targets 30. The cap applies from round 8; rounds remain unlimited. A standard round has five hands, the Hands deck six; the Life deck keeps one whole-run revive and Coins starts with 10 coins. Dealer stands on soft or hard 17. Natural, push, loss, shuffle, and existing upgrade rules are unchanged. Multipliers add their bonuses rather than multiplying each other. Losses and pushes pay no reward.

The shop first draws a rarity with the stated weights, then an eligible upgrade within that rarity. Owned items and duplicate current offers are excluded; a fresh item within the chosen rarity is preferred. Every tier has enough entries to preserve its weight even with eight equipped items. Buying and selling never advance card randomness. An atomic replacement validates offer, ownership, visit purchase marker and funds including sale credit before changing one existing slot. Invalid commands preserve state and randomness. A sold purchase cannot be bought again in the same shop.

## Active controls and limits

| Ability | Charge | Preconditions | Result |
| --- | --- | --- | --- |
| Rewind | 1 per hand | Your turn, at least one hit | Discard last hit, including a pending bust |
| Redraw | 1 per hand | Your turn, at least one hit | Replace last hit with the next deck card; replacement may bust |
| Forked Path | 1 per hand | Your turn, no pending bust or choice | Draw two, choose one as next hit, discard the other |
| Lifeline | 1 per round | A pending bust before settlement | Discard the bust card |

Rewind and Redraw cannot alter the two opening cards or manufacture a natural blackjack. Hitting 21 still settles through the normal dealer turn. A bust pauses only when Rewind, Redraw or Lifeline is equipped, charged and applicable; the dealer hole card remains hidden and no score is awarded. Stand explicitly accepts that loss. A second bust after all charges are spent settles normally. The older Second Chance automatic shield has priority and still works once per round.

An open Forked Path choice must be resolved before any hit, stand, or other ability. Invalid selections, repeated clicks, unowned abilities, passives and actions after settlement do nothing. Charges are consumed when an ability activates (including when the two-card choice opens), not when its visual animation completes. New hands reset hand charges and synergy flags. New rounds or the Life deck retry reset round charges. Restart clears all equipment, choices, charges and flags. No save/load system exists in this game, so there is no persistent inventory schema to migrate.

Alchemist pays only on a win after an active use in the same hand. Phoenix pays only after that hand actually escaped a bust via Second Chance, Rewind, Redraw or Lifeline. Neither pays on a loss or push; neither can reopen a settled hand.

## Complete catalog

| Upgrade | Rarity | Cost | Effect |
| --- | --- | ---: | --- |
| Precision | Rare | 6 | Win with 21: +0.5 multiplier and +1 coin. |
| Long Hand | Rare | 6 | Win with 4+ cards: +0.6 multiplier and +1 coin. |
| Second Chance | Legendary | 10 | Rescue one bust per round. Winning after that rescue adds +1.0 multiplier. |
| Streak | Rare | 6 | +0.1 multiplier per earlier consecutive win (max +0.4). Every third win earns +1 coin. |
| Royal Purse | Common | 3 | Win with a natural blackjack for +1 coin. |
| Soft Touch | Common | 3 | Win with a soft hand for +1 coin. |
| Narrow Escape | Rare | 6 | Win when the dealer busts: +0.4 multiplier and +1 coin. |
| Red Ribbon | Common | 3 | Win with two red opening cards for +1 coin. |
| Black Velvet | Common | 3 | Win with two black opening cards for +1 coin. |
| Even Odds | Common | 3 | Win with an even hand total for +1 coin. |
| Court Favor | Common | 3 | Win with at least one J, Q or K for +3 points. |
| Humble Crown | Common | 3 | Win with 18 or less for +5 points. |
| Blackjack Bonus | Common | 3 | Win with a natural blackjack for +5 points. |
| Copper Bell | Common | 3 | Win with 3+ cards for +3 points. |
| Seventeen Charm | Common | 3 | Win with exactly 17 for +1 coin. |
| Scarlet Mask | Rare | 6 | Win with an entirely red hand: +0.5 multiplier. |
| Midnight Mask | Rare | 6 | Win with an entirely black hand: +0.5 multiplier. |
| Royal Court | Rare | 6 | Win with 2+ face cards: +0.75 multiplier. |
| Soft Crown | Rare | 6 | Win with a soft 19, 20 or 21: +0.75 multiplier. |
| Jester Crown | Legendary | 10 | Win with a natural blackjack: +1.0 multiplier. |
| Five Card Parade | Legendary | 10 | Win with 5+ cards: +1.5 multiplier. |
| Final Act | Legendary | 10 | Win on the round's last available hand: +1.0 multiplier. |
| Underdog | Legendary | 10 | Win with 16 or less when the dealer busts: +1.5 multiplier. |
| Encore | Legendary | 10 | Win after 3+ earlier consecutive wins: +1.0 multiplier. |
| Rewind | Rare | 6 | ACTIVE: discard your last hit, including a bust card. Once per hand; opening cards cannot be removed. |
| Redraw | Rare | 6 | ACTIVE: replace your last hit with the next card. Once per hand; the replacement can still bust. |
| Forked Path | Legendary | 10 | ACTIVE: draw two cards and choose one as your next hit. The other is discarded. Once per hand. |
| Lifeline | Legendary | 10 | ACTIVE: discard a bust card before the hand settles. Once per round; recharges next round. |
| Velvet Dividend | Common | 3 | Every win earns +2 coins. |
| Lucky Seven | Common | 3 | Win with at least one seven: +7 points. |
| Twin Sigil | Common | 3 | Win with a matching opening pair: +10 points. |
| Mixed Company | Common | 3 | Win with both red and black cards: +5 points. |
| Crescendo | Rare | 6 | Win with 3+ cards: +0.75 multiplier. |
| Ace High | Rare | 6 | Win with any ace: +0.75 multiplier and +1 coin. |
| Silver Lining | Rare | 6 | Every win adds +0.35 multiplier. |
| Loaded Dice | Rare | 6 | Win with 18 or more: +8 points. |
| Alchemist | Rare | 6 | Win after using an active ability: +0.8 multiplier and +2 coins. |
| Phoenix | Legendary | 10 | Win after escaping a bust with any rescue: +2.0 multiplier and +2 coins. |
| Royal Decree | Legendary | 10 | Every win adds +0.8 multiplier. |
| Grand Finale | Legendary | 10 | Win with at most one hand remaining: +1.5 multiplier. |

## Verification

The core rules suite tests all equipment counts 0–8 through legal seeded play, capacity rejection, atomic full-slot replacement and insufficient-funds rejection, acquisition, sale and restart, active eligibility and charges, both card choices, deferred bust settlement, deterministic replay and actual synergy payout. Existing opening-distribution, weighted-shop, full-deck uniqueness, outcome, reward, shield and starting-deck tests remain. See [balance-results.md](balance-results.md) for the independently seeded before/after progression comparison and its limitations.
