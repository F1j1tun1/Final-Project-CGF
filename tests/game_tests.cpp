#include "game/Game.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

int failures = 0;
int checks = 0;

void check(bool condition, const std::string& message) {
    ++checks;
    if (!condition) {
        ++failures;
        if (failures <= 25) {
            std::cerr << "FAIL: " << message << '\n';
        }
    }
}

std::vector<bj::Card> hand(std::initializer_list<int> ranks) {
    std::vector<bj::Card> cards;
    for (int rank : ranks) {
        cards.push_back({rank, bj::Suit::Clubs});
    }
    return cards;
}

bool sameCards(const std::vector<bj::Card>& a, const std::vector<bj::Card>& b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](const bj::Card& x, const bj::Card& y) {
        return x.rank == y.rank && x.suit == y.suit;
    });
}

bool sameState(const bj::Snapshot& a, const bj::Snapshot& b) {
    return sameCards(a.player, b.player) && sameCards(a.dealer, b.dealer)
        && a.phase == b.phase && a.outcome == b.outcome && a.round == b.round
        && a.handsLeft == b.handsLeft && a.roundScore == b.roundScore && a.goal == b.goal
        && a.wins == b.wins && a.totalScore == b.totalScore
        && a.coins == b.coins && a.streak == b.streak && a.dealerStand == b.dealerStand
        && a.seed == b.seed && a.usedSecondChance == b.usedSecondChance && a.owned == b.owned
        && a.offers == b.offers && a.score.base == b.score.base && a.score.bonus == b.score.bonus
        && a.score.multiplier == b.score.multiplier && a.score.total == b.score.total
        && a.score.coins == b.score.coins && a.message == b.message;
}

bool contains(const std::vector<bj::Relic>& relics, bj::Relic relic) {
    return std::find(relics.begin(), relics.end(), relic) != relics.end();
}

bj::Rarity rarityOf(bj::Relic relic) {
    const auto& catalog = bj::relicCatalog();
    return std::find_if(catalog.begin(), catalog.end(), [relic](const bj::RelicInfo& item) { return item.id == relic; })->rarity;
}

void checkBlockedActions(bj::Game& game) {
    const auto original = game.state();
    const auto phase = original.phase;
    if (phase != bj::Phase::Ready) {
        check(!game.deal(), "deal is restricted to Ready");
    }
    if (phase != bj::Phase::PlayerTurn) {
        check(!game.hit(), "hit is restricted to PlayerTurn");
        check(!game.stand(), "stand is restricted to PlayerTurn");
    }
    if (phase != bj::Phase::RoundOver) {
        check(!game.advance(), "advance is restricted to RoundOver");
    }
    if (phase != bj::Phase::Shop) {
        check(!game.buy(bj::Relic::Precision), "buy is restricted to Shop");
        check(!game.sell(bj::Relic::Precision), "sell is restricted to Shop");
        check(!game.leaveShop(), "leaveShop is restricted to Shop");
    }
    check(!game.buy(static_cast<bj::Relic>(99)), "unknown upgrade cannot be bought");
    check(!game.sell(static_cast<bj::Relic>(99)), "unknown upgrade cannot be sold");
    check(sameState(original, game.state()), "blocked commands preserve every snapshot field");
}

void testHands() {
    auto value = bj::evaluateHand({});
    check(value.total == 0 && !value.soft && !value.blackjack && !value.bust, "empty hand");
    value = bj::evaluateHand(hand({1, 13}));
    check(value.total == 21 && value.soft && value.blackjack && !value.bust, "ace + king is natural blackjack");
    value = bj::evaluateHand(hand({1, 6}));
    check(value.total == 17 && value.soft, "soft 17");
    value = bj::evaluateHand(hand({1, 6, 10}));
    check(value.total == 17 && !value.soft, "ace drops from 11 to 1");
    value = bj::evaluateHand(hand({1, 1, 9}));
    check(value.total == 21 && value.soft && !value.blackjack, "three-card 21 is not natural");
    value = bj::evaluateHand(hand({1, 1, 1, 9}));
    check(value.total == 12 && !value.soft && !value.bust, "multiple aces drop together");
    value = bj::evaluateHand(hand({10, 11, 12}));
    check(value.total == 30 && value.bust && !value.soft, "face cards count as ten");
    value = bj::evaluateHand(hand({1, 1, 1, 1, 7}));
    check(value.total == 21 && value.soft, "four aces can retain one soft ace");
    for (int rank : {0, 14}) {
        bool rejected = false;
        try {
            bj::evaluateHand(hand({rank}));
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected, "invalid rank is rejected");
    }
}

// Independent comparison includes the priority of a natural over an ordinary 21.
bj::Outcome expectedOutcome(const bj::Snapshot& state) {
    const auto player = bj::evaluateHand(state.player);
    const auto dealer = bj::evaluateHand(state.dealer);
    if (player.bust) return bj::Outcome::Loss;
    if (dealer.blackjack) return player.blackjack ? bj::Outcome::Push : bj::Outcome::Loss;
    if (player.blackjack || dealer.bust) return bj::Outcome::Win;
    if (player.total == dealer.total) return bj::Outcome::Push;
    return player.total > dealer.total ? bj::Outcome::Win : bj::Outcome::Loss;
}

int resultRank(const bj::Snapshot& state) {
    const int outcome = state.outcome == bj::Outcome::Win ? 2 : (state.outcome == bj::Outcome::Push ? 1 : 0);
    return outcome * 10000 + state.score.coins * 100 + state.score.total;
}

// Explore legal continuations on copies to exercise later shops and upgrades.
// This is only a test search policy, not the strategy shipped in the game.
bj::Game finishBest(bj::Game game) {
    if (game.state().phase == bj::Phase::RoundOver || game.state().phase == bj::Phase::RunLost) return game;
    bj::Game best = game;
    int bestRank = -1;
    for (int action = 0; action < 52; ++action) {
        if (game.state().phase == bj::Phase::RoundOver || game.state().phase == bj::Phase::RunLost) {
            if (resultRank(game.state()) > bestRank) best = game;
            break;
        }
        auto standing = game;
        standing.stand();
        if (resultRank(standing.state()) > bestRank) {
            best = standing;
            bestRank = resultRank(standing.state());
        }
        game.hit();
    }
    return best;
}

void replayStep(bj::Game& game) {
    switch (game.state().phase) {
    case bj::Phase::Ready: game.deal(); break;
    case bj::Phase::PlayerTurn:
        if (bj::evaluateHand(game.state().player).total < 17) game.hit();
        else game.stand();
        break;
    case bj::Phase::RoundOver: game.advance(); break;
    case bj::Phase::Shop:
        for (auto relic : game.state().offers) game.buy(relic);
        game.leaveShop();
        break;
    case bj::Phase::RunLost: game.restart(game.state().seed + 1); break;
    }
}

void testDeterminismAndBlockedRandomness() {
    bj::Game first(112358);
    bj::Game second(112358);
    const auto initial = first.state();
    for (int action = 0; action < 1000; ++action) {
        checkBlockedActions(first);
        check(sameState(first.state(), second.state()), "blocked commands preserve observable state");
        replayStep(first);
        replayStep(second);
        check(sameState(first.state(), second.state()), "blocked commands preserve future card and shop randomness");
    }
    first.restart(112358);
    second.restart(112358);
    check(sameState(initial, first.state()), "restart clears the complete run state");
    first.deal();
    second.deal();
    check(sameState(first.state(), second.state()), "restart resets both random streams");
    bj::Game other(112359);
    other.deal();
    check(!sameCards(first.state().player, other.state().player)
        || !sameCards(first.state().dealer, other.state().dealer), "different seeds change the opening deal");
}

void testNaturalBaseAndActualBust() {
    bool naturalWin = false;
    bool naturalPush = false;
    bool actualBust = false;
    for (std::uint32_t seed = 0; seed < 10000 && !(naturalWin && naturalPush && actualBust); ++seed) {
        bj::Game game(seed);
        game.deal();
        const auto result = game.state();
        const auto player = bj::evaluateHand(result.player);
        if (!naturalWin && player.blackjack && result.outcome == bj::Outcome::Win) {
            naturalWin = true;
            check(result.score.base == 21 && result.score.bonus == 0 && result.score.total == 21
                && result.roundScore == 21 && result.totalScore == 21, "an unupgraded natural scores exactly twenty-one, without an automatic bonus");
            check(result.score.coins == 3 && result.coins == 3 && result.handsLeft == 4,
                "a natural pays normal win income and consumes one hand");
            check(result.phase == bj::Phase::RoundOver && game.advance()
                && game.state().phase == bj::Phase::Ready && game.state().roundScore == 21,
                "a first-hand natural alone does not clear the thirty-point round goal");
        }
        if (!naturalPush && player.blackjack && result.outcome == bj::Outcome::Push) {
            naturalPush = true;
            check(result.score.base == 0 && result.score.bonus == 0 && result.score.total == 0
                && result.score.coins == 0 && result.roundScore == 0, "two natural blackjacks push and award no points or coins");
        }
        if (!actualBust && result.phase == bj::Phase::PlayerTurn) {
            auto probe = game;
            for (int action = 0; action < 52 && probe.state().phase == bj::Phase::PlayerTurn; ++action) {
                probe.hit();
                const auto value = bj::evaluateHand(probe.state().player);
                if (value.bust) {
                    actualBust = true;
                    check(value.total > 21 && probe.state().outcome == bj::Outcome::Loss
                        && probe.state().score.base == 0 && probe.state().score.bonus == 0
                        && probe.state().score.total == 0 && probe.state().score.coins == 0,
                        "an actual hit bust retains its real above-twenty-one total and awards nothing");
                    check(probe.state().phase == bj::Phase::RoundOver && probe.state().handsLeft == 4
                        && probe.advance() && probe.state().phase == bj::Phase::Ready,
                        "an early bust ends only that hand and retains the other four attempts");
                }
            }
        }
    }
    check(naturalWin && naturalPush && actualBust, "bounded legal seed search covers a natural win, natural push and actual bust");
}

void testBlackjackBonusCounterfactual() {
    int naturalWins = 0;
    int ordinaryTwentyOnes = 0;
    int losses = 0;
    int pushes = 0;
    int comparedHands = 0;
    for (std::uint32_t seed = 0; seed < 5000
        && (naturalWins == 0 || ordinaryTwentyOnes == 0 || losses == 0 || pushes == 0); ++seed) {
        bj::Game plain(seed);
        bool offered = false;
        for (int attempt = 0; attempt < 30 && plain.state().phase == bj::Phase::Ready; ++attempt) {
            plain.deal();
            plain = finishBest(plain);
            if (plain.state().phase == bj::Phase::RunLost) break;
            plain.advance();
            if (plain.state().phase != bj::Phase::Shop) continue;
            offered = contains(plain.state().offers, bj::Relic::BlackjackBonus) && plain.state().coins >= 4;
            if (offered) break;
            plain.leaveShop();
        }
        if (!offered) continue;
        auto upgraded = plain;
        check(upgraded.buy(bj::Relic::BlackjackBonus) && upgraded.state().owned.size() == 1,
            "the counterfactual run acquires Blackjack Bonus through its real shop offer");
        plain.leaveShop();
        upgraded.leaveShop();
        int accumulatedBonus = 0;
        for (int attempt = 0; attempt < 5; ++attempt) {
            check(plain.deal() && upgraded.deal(), "both comparison runs deal through the public command API");
            for (int action = 0; action < 52 && plain.state().phase == bj::Phase::PlayerTurn
                && upgraded.state().phase == bj::Phase::PlayerTurn; ++action) {
                if (bj::evaluateHand(plain.state().player).total < 17) {
                    plain.hit();
                    upgraded.hit();
                } else {
                    plain.stand();
                    upgraded.stand();
                }
            }
            const auto baseline = plain.state();
            const auto boosted = upgraded.state();
            const auto player = bj::evaluateHand(baseline.player);
            const bool eligible = baseline.outcome == bj::Outcome::Win && player.blackjack;
            const int extraPoints = eligible ? 5 : 0;
            accumulatedBonus += extraPoints;
            ++comparedHands;
            check(sameCards(baseline.player, boosted.player) && sameCards(baseline.dealer, boosted.dealer)
                && baseline.outcome == boosted.outcome, "buying the natural bonus never changes dealt cards or the winner");
            check(boosted.score.base == baseline.score.base && boosted.score.bonus == baseline.score.bonus + extraPoints
                && boosted.score.total == baseline.score.total + extraPoints,
                "Blackjack Bonus adds five points only to a winning two-card natural");
            check(boosted.score.coins == baseline.score.coins && boosted.coins == baseline.coins - 4,
                "Blackjack Bonus costs four coins and never adds extra win income");
            check(boosted.roundScore == baseline.roundScore + accumulatedBonus
                && boosted.totalScore == baseline.totalScore + accumulatedBonus,
                "only the earned natural bonus changes accumulated round and run points");
            naturalWins += eligible ? 1 : 0;
            ordinaryTwentyOnes += baseline.outcome == bj::Outcome::Win && player.total == 21 && !player.blackjack ? 1 : 0;
            losses += baseline.outcome == bj::Outcome::Loss ? 1 : 0;
            pushes += baseline.outcome == bj::Outcome::Push ? 1 : 0;
            checkBlockedActions(upgraded);
            if (baseline.phase == bj::Phase::RunLost || boosted.phase == bj::Phase::RunLost
                || baseline.roundScore >= baseline.goal || boosted.roundScore >= boosted.goal) break;
            check(plain.advance() && upgraded.advance() && plain.state().phase == bj::Phase::Ready
                && upgraded.state().phase == bj::Phase::Ready, "comparison hands continue normally below the goal");
        }
    }
    check(naturalWins > 0 && ordinaryTwentyOnes > 0 && losses > 0 && pushes > 0,
        "bounded shop-and-play search tests the bonus on naturals, ordinary winning twenty-ones, losses and pushes");
    std::cout << "Blackjack Bonus comparison: " << comparedHands << " hands; natural wins " << naturalWins
              << ", ordinary winning twenty-ones " << ordinaryTwentyOnes << ", losses " << losses << ", pushes " << pushes << '\n';
}

void testUnbiasedOpeningDistribution() {
    constexpr std::uint32_t samples = 40000;
    int naturals = 0;
    int dealerNaturals = 0;
    std::array<int, 13> ranks{};
    std::array<int, 4> suits{};
    for (std::uint32_t seed = 0; seed < samples; ++seed) {
        bj::Game game(seed);
        game.deal();
        naturals += bj::evaluateHand(game.state().player).blackjack ? 1 : 0;
        dealerNaturals += bj::evaluateHand(game.state().dealer).blackjack ? 1 : 0;
        bool seen[52] = {};
        for (const auto* cards : {&game.state().player, &game.state().dealer}) {
            for (const auto& card : *cards) {
                const int suit = static_cast<int>(card.suit);
                const int id = suit * 13 + card.rank - 1;
                check(id >= 0 && id < 52 && !seen[id], "opening deals use unique standard-deck cards");
                if (id >= 0 && id < 52) {
                    seen[id] = true;
                    ++ranks[static_cast<std::size_t>(card.rank - 1)];
                    ++suits[static_cast<std::size_t>(suit)];
                }
            }
        }
    }
    // Standard-deck P(natural) = 2 * 4/52 * 16/51, approximately 4.83%.
    const double frequency = static_cast<double>(naturals) / samples;
    const double dealerFrequency = static_cast<double>(dealerNaturals) / samples;
    check(frequency > 0.042 && frequency < 0.055, "player naturals are near the standard 4.83 percent");
    check(dealerFrequency > 0.042 && dealerFrequency < 0.055, "dealer naturals are near the standard 4.83 percent");
    for (int count : ranks) check(count > 11500 && count < 13100, "no preferred rank in opening deals");
    for (int count : suits) check(count > 39000 && count < 41000, "no preferred suit in opening deals");
    std::cout << "Opening naturals: player " << naturals << '/' << samples << " ("
              << std::fixed << std::setprecision(3) << frequency * 100.0 << "%), dealer "
              << dealerNaturals << '/' << samples << " (" << dealerFrequency * 100.0 << "%)\n";
}

void testRarityDistribution() {
    constexpr std::array<bj::Rarity, 5> rarities{
        bj::Rarity::Common, bj::Rarity::Uncommon, bj::Rarity::Rare, bj::Rarity::Epic, bj::Rarity::Legendary
    };
    constexpr std::array<int, 5> weights{50, 28, 14, 6, 2};
    constexpr std::array<const char*, 5> names{"Common", "Uncommon", "Rare", "Epic", "Legendary"};
    constexpr std::array<int, 5> tierSizes{4, 4, 3, 1, 1};
    constexpr std::array<bj::Rarity, 13> assignments{
        bj::Rarity::Rare, bj::Rarity::Rare, bj::Rarity::Legendary, bj::Rarity::Epic,
        bj::Rarity::Common, bj::Rarity::Common, bj::Rarity::Rare, bj::Rarity::Uncommon,
        bj::Rarity::Uncommon, bj::Rarity::Uncommon, bj::Rarity::Common, bj::Rarity::Common,
        bj::Rarity::Uncommon
    };
    constexpr std::array<int, 13> costs{6, 6, 10, 8, 2, 2, 6, 4, 4, 4, 2, 2, 4};
    for (std::size_t tier = 0; tier < rarities.size(); ++tier) {
        check(bj::rarityWeight(rarities[tier]) == weights[tier] && std::string(bj::rarityName(rarities[tier])) == names[tier],
            "rarity metadata exposes the documented names and base weights");
    }
    check(bj::rarityWeight(static_cast<bj::Rarity>(99)) == 0
        && std::string(bj::rarityName(static_cast<bj::Rarity>(99))) == "Unknown", "unknown rarity metadata is harmless");
    for (const auto& item : bj::relicCatalog()) {
        const auto index = static_cast<std::size_t>(item.id);
        check(item.rarity == assignments[index] && item.cost == costs[index], "each upgrade has its agreed rarity and purchase price");
    }

    int shops = 0;
    std::array<int, 5> firstCounts{};
    std::array<int, 13> itemCounts{};
    std::array<std::array<int, 5>, 5> secondCounts{};
    for (std::uint32_t seed = 0; seed < 40000; ++seed) {
        bj::Game game(seed);
        for (int attempt = 0; attempt < 5 && game.state().phase == bj::Phase::Ready; ++attempt) {
            game.deal();
            game = finishBest(game);
            if (game.state().phase == bj::Phase::RunLost) break;
            game.advance();
            if (game.state().phase != bj::Phase::Shop) continue;
            const auto& offers = game.state().offers;
            check(offers.size() == 2 && offers[0] != offers[1], "weighted first shops contain two distinct choices");
            if (offers.size() != 2) break;
            const auto first = static_cast<std::size_t>(rarityOf(offers[0]));
            const auto second = static_cast<std::size_t>(rarityOf(offers[1]));
            ++firstCounts[first];
            ++itemCounts[static_cast<std::size_t>(offers[0])];
            ++secondCounts[first][second];
            ++shops;
        }
    }
    check(shops > 25000, "distribution test reaches enough first shops through legal play");
    for (std::size_t tier = 0; tier < rarities.size(); ++tier) {
        const double probability = static_cast<double>(weights[tier]) / 100.0;
        const double expected = shops * probability;
        const double tolerance = 6.0 * std::sqrt(shops * probability * (1.0 - probability)) + 10.0;
        check(std::abs(firstCounts[tier] - expected) < tolerance,
            std::string(names[tier]) + " first-slot frequency follows its tier weight, independent of tier size");

        // The first choice removes Epic or Legendary's singleton from slot two.
        // Other tiers retain their base weight because they still contain eligible items.
        const int remainingWeight = 100 - (tierSizes[tier] == 1 ? weights[tier] : 0);
        for (std::size_t second = 0; second < rarities.size(); ++second) {
            const bool unavailable = second == tier && tierSizes[tier] == 1;
            if (unavailable) {
                check(secondCounts[tier][second] == 0, "second slot never duplicates a singleton Epic or Legendary choice");
                continue;
            }
            const double conditional = static_cast<double>(weights[second]) / remainingWeight;
            const double conditionalExpected = firstCounts[tier] * conditional;
            const double conditionalTolerance = 6.0 * std::sqrt(firstCounts[tier] * conditional * (1.0 - conditional)) + 10.0;
            check(std::abs(secondCounts[tier][second] - conditionalExpected) < conditionalTolerance,
                "slot two renormalizes rarity weights only when a tier has no eligible item left");
        }
    }
    for (const auto& item : bj::relicCatalog()) {
        const auto tier = static_cast<std::size_t>(item.rarity);
        const double probability = static_cast<double>(weights[tier]) / (100.0 * tierSizes[tier]);
        const double expected = shops * probability;
        const double tolerance = 6.0 * std::sqrt(shops * probability * (1.0 - probability)) + 10.0;
        check(std::abs(itemCounts[static_cast<std::size_t>(item.id)] - expected) < tolerance,
            std::string(item.name) + " is uniform among the fresh items inside its selected rarity");
    }
    std::cout << "First-shop rarity samples: " << shops;
    for (std::size_t tier = 0; tier < rarities.size(); ++tier) {
        std::cout << "; " << names[tier] << ' ' << firstCounts[tier] << " ("
                  << std::fixed << std::setprecision(3) << 100.0 * firstCounts[tier] / shops << "%)";
    }
    std::cout << '\n';
}

struct Coverage {
    int wins = 0, pushes = 0, losses = 0, soft17 = 0, longRuns = 0;
    int clearedRounds = 0, failedRounds = 0, lastHandClears = 0, earlyClears = 0;
    int lossContinues = 0, pushContinues = 0, winContinues = 0, lastHandWinFails = 0;
    int cappedGoals = 0, shieldResets = 0, maxRound = 1;
    int streakBoosts = 0, streakCaps = 0;
    int naturalPrecisionStacks = 0, naturalStreakStacks = 0;
    int purchases = 0, sales = 0, reoffers = 0, fullBuilds = 0, cardStreams = 0;
    int shields = 0, spentShields = 0, persistentShields = 0;
    std::array<int, 13> active{};
    std::array<int, 13> inactive{};
};

bool red(const bj::Card& card) {
    return card.suit == bj::Suit::Diamonds || card.suit == bj::Suit::Hearts;
}

void checkRewards(const bj::Snapshot& before, const bj::Snapshot& result, Coverage& coverage) {
    const auto player = bj::evaluateHand(result.player);
    const auto dealer = bj::evaluateHand(result.dealer);
    if (result.outcome != bj::Outcome::Win) {
        check(result.score.base == 0 && result.score.bonus == 0 && result.score.total == 0
            && result.score.coins == 0 && result.score.multiplier == 1.0f, "non-wins award no points or coins");
        check(result.coins == before.coins && result.totalScore == before.totalScore && result.roundScore == before.roundScore
            && result.wins == before.wins, "non-wins leave accumulated rewards unchanged");
        check(result.streak == (result.outcome == bj::Outcome::Loss ? 0 : before.streak), "loss breaks streak; push preserves it");
        return;
    }
    int expectedCoins = 3;
    int expectedBonus = 0;
    for (auto relic : before.owned) {
        bool active = false;
        int bonus = 0;
        int coinBonus = 1;
        switch (relic) {
        case bj::Relic::Precision: active = player.total == 21; bonus = 5; break;
        case bj::Relic::LongHand: active = result.player.size() >= 4; bonus = 8; break;
        case bj::Relic::Streak: active = (before.streak + 1) % 3 == 0; break;
        case bj::Relic::RoyalPurse: active = player.blackjack; break;
        case bj::Relic::SoftTouch: active = player.soft; break;
        case bj::Relic::NarrowEscape: active = dealer.bust; bonus = 3; break;
        case bj::Relic::RedRibbon: active = red(result.player[0]) && red(result.player[1]); break;
        case bj::Relic::BlackVelvet: active = !red(result.player[0]) && !red(result.player[1]); break;
        case bj::Relic::EvenOdds: active = player.total % 2 == 0; break;
        case bj::Relic::CourtFavor:
            active = std::any_of(result.player.begin(), result.player.end(), [](const bj::Card& card) { return card.rank > 10; });
            bonus = 3;
            coinBonus = 0;
            break;
        case bj::Relic::HumbleCrown: active = player.total <= 18; bonus = 5; coinBonus = 0; break;
        case bj::Relic::BlackjackBonus: active = player.blackjack; bonus = 5; coinBonus = 0; break;
        case bj::Relic::SecondChance: continue;
        }
        const auto index = static_cast<std::size_t>(relic);
        ++(active ? coverage.active[index] : coverage.inactive[index]);
        if (active) {
            expectedCoins += coinBonus;
            expectedBonus += bonus;
        }
    }
    const std::array<float, 5> streakMultipliers{1.0f, 1.1f, 1.2f, 1.3f, 1.4f};
    const float expectedMultiplier = contains(before.owned, bj::Relic::Streak)
        ? streakMultipliers[static_cast<std::size_t>(std::min(before.streak, 4))] : 1.0f;
    coverage.streakBoosts += expectedMultiplier > 1.0f ? 1 : 0;
    coverage.streakCaps += contains(before.owned, bj::Relic::Streak) && before.streak > 4 ? 1 : 0;
    const bool naturalBonus = player.blackjack && contains(before.owned, bj::Relic::BlackjackBonus);
    coverage.naturalPrecisionStacks += naturalBonus && contains(before.owned, bj::Relic::Precision) ? 1 : 0;
    coverage.naturalStreakStacks += naturalBonus && expectedMultiplier > 1.0f ? 1 : 0;
    const int expectedTotal = static_cast<int>(std::lround(static_cast<float>(player.total + expectedBonus) * expectedMultiplier));
    check(result.score.base == player.total && result.score.bonus == expectedBonus
        && result.score.total == expectedTotal && std::abs(result.score.multiplier - expectedMultiplier) < 0.00001f,
        "points add only owned relic bonuses before applying the capped streak multiplier");
    check(result.score.coins == expectedCoins && result.coins == before.coins + expectedCoins,
        "win income is three coins plus eligible modest bonuses");
    check(result.totalScore == before.totalScore + result.score.total && result.roundScore == before.roundScore + result.score.total
        && result.wins == before.wins + 1, "winning points accumulate once in the round and complete run");
    check(result.streak == before.streak + 1, "win increments streak exactly once");
}

void probeShield(bj::Game game, Coverage& coverage) {
    if (!game.hasRelic(bj::Relic::SecondChance) || game.state().phase != bj::Phase::PlayerTurn) return;
    for (int action = 0; game.state().phase == bj::Phase::PlayerTurn && action < 52; ++action) {
        const auto before = game.state();
        game.hit();
        if (!before.usedSecondChance && game.state().usedSecondChance) {
            ++coverage.shields;
            check(sameCards(before.player, game.state().player), "shield discards exactly the new bust card");
            check(game.state().phase == bj::Phase::PlayerTurn && game.state().coins == before.coins
                && game.state().totalScore == before.totalScore, "shield rescue does not award a win");
        }
        if (before.usedSecondChance && (game.state().phase == bj::Phase::RoundOver || game.state().phase == bj::Phase::RunLost)
            && bj::evaluateHand(game.state().player).bust) {
            ++coverage.spentShields;
            check(game.state().outcome == bj::Outcome::Loss, "spent shield cannot rescue another bust");
        }
    }
}

void testSeededRunsAndShop() {
    Coverage coverage;
    const auto& catalog = bj::relicCatalog();
    check(catalog.size() == 13, "catalogue has thirteen choices");
    for (const auto& item : catalog) {
        check(item.cost > 0 && item.cost <= 10 && item.cost % 2 == 0
            && item.name[0] != '\0' && item.description[0] != '\0', "bounded even prices and descriptive text");
        check(std::count_if(catalog.begin(), catalog.end(), [&](const bj::RelicInfo& other) { return item.id == other.id; }) == 1,
            "unique catalogue identifiers");
    }
    for (std::uint32_t seed = 0; seed < 3000; ++seed) {
        bj::Game game(seed);
        check(game.state().phase == bj::Phase::Ready && game.state().round == 1
            && game.state().handsLeft == 5 && game.state().roundScore == 0 && game.state().goal == 30
            && game.state().coins == 0 && game.state().wins == 0 && game.state().totalScore == 0
            && game.state().dealerStand == 17 && game.state().owned.empty() && game.state().offers.empty(), "new run defaults");
        std::vector<bj::Relic> previousOffers;
        std::vector<bj::Relic> sold;
        for (int dealtHand = 0; dealtHand < 500; ++dealtHand) {
            checkBlockedActions(game);
            const auto beforeRound = game.state();
            check(game.deal(), "deal succeeds in Ready");
            check(game.state().handsLeft == beforeRound.handsLeft - 1, "every dealt hand spends one of the five attempts");
            check(game.state().player.size() == 2 && game.state().dealer.size() == 2, "opening deal gives two cards to each side");
            check(game.state().usedSecondChance == beforeRound.usedSecondChance, "deal does not recharge shield");
            probeShield(game, coverage);
            game = finishBest(game);
            const auto result = game.state();
            check((result.phase == bj::Phase::RoundOver || result.phase == bj::Phase::RunLost)
                && result.outcome == expectedOutcome(result), "hand resolves by blackjack comparison");
            const auto player = bj::evaluateHand(result.player);
            const auto dealer = bj::evaluateHand(result.dealer);
            bool seen[52] = {};
            for (const auto* cards : {&result.player, &result.dealer}) {
                for (const auto& card : *cards) {
                    const int id = static_cast<int>(card.suit) * 13 + card.rank - 1;
                    check(id >= 0 && id < 52 && !seen[id], "complete round uses unique standard-deck cards");
                    if (id >= 0 && id < 52) seen[id] = true;
                }
            }
            if (!player.bust && !player.blackjack && !dealer.blackjack) {
                check(dealer.total >= 17, "dealer draws to hard or soft 17");
                if (result.dealer.size() > 2) {
                    auto previous = result.dealer;
                    previous.pop_back();
                    check(bj::evaluateHand(previous).total < 17, "dealer stops as soon as threshold is reached");
                }
                coverage.soft17 += dealer.total == 17 && dealer.soft ? 1 : 0;
            }
            checkRewards(beforeRound, result, coverage);
            coverage.wins += result.outcome == bj::Outcome::Win ? 1 : 0;
            coverage.pushes += result.outcome == bj::Outcome::Push ? 1 : 0;
            coverage.losses += result.outcome == bj::Outcome::Loss ? 1 : 0;
            coverage.longRuns += result.round > 5 ? 1 : 0;
            coverage.maxRound = std::max(coverage.maxRound, result.round);
            coverage.cappedGoals += result.round >= 8 && result.goal == 100 ? 1 : 0;
            checkBlockedActions(game);
            if (result.roundScore < result.goal && result.handsLeft == 0) {
                ++coverage.failedRounds;
                coverage.lastHandWinFails += result.outcome == bj::Outcome::Win ? 1 : 0;
                check(game.state().phase == bj::Phase::RunLost, "unmet goal after all five hands ends the run immediately");
                checkBlockedActions(game);
                game.restart(seed);
                const bj::Game fresh(seed);
                check(sameState(fresh.state(), game.state()), "restart after defeat clears full run state");
                break;
            }
            check(result.phase == bj::Phase::RoundOver, "a surviving hand or final-hand goal clear keeps its result screen");
            check(game.advance(), "advance succeeds after a surviving hand");
            check(game.state().coins == result.coins && game.state().totalScore == result.totalScore
                && game.state().roundScore == result.roundScore && game.state().handsLeft == result.handsLeft,
                "advance never repeats rewards or spends another hand");
            if (result.roundScore < result.goal) {
                coverage.lossContinues += result.outcome == bj::Outcome::Loss ? 1 : 0;
                coverage.pushContinues += result.outcome == bj::Outcome::Push ? 1 : 0;
                coverage.winContinues += result.outcome == bj::Outcome::Win ? 1 : 0;
                coverage.persistentShields += result.usedSecondChance ? 1 : 0;
                check(game.state().phase == bj::Phase::Ready && game.state().round == result.round
                    && game.state().offers.empty() && game.state().usedSecondChance == result.usedSecondChance,
                    "a hand below the target continues the same round without opening a shop or recharging a shield");
                continue;
            }
            ++coverage.clearedRounds;
            coverage.lastHandClears += result.handsLeft == 0 ? 1 : 0;
            coverage.earlyClears += result.handsLeft > 0 ? 1 : 0;
            check(game.state().phase == bj::Phase::Shop, "only a cleared round opens the shop, including a last-hand clear");
            const auto offers = game.state().offers;
            check(offers.size() == 2 && offers[0] != offers[1], "shop offers exactly two distinct upgrades");
            std::vector<bj::Relic> earlierSlots;
            for (auto relic : offers) {
                check(!game.hasRelic(relic), "shops do not offer an active upgrade");
                if (contains(previousOffers, relic)) {
                    check(std::none_of(catalog.begin(), catalog.end(), [&](const bj::RelicInfo& item) {
                        return item.rarity == rarityOf(relic) && !game.hasRelic(item.id)
                            && !contains(previousOffers, item.id) && !contains(earlierSlots, item.id);
                    }), "an earlier offer repeats only when its selected rarity has no fresh eligible item");
                }
                earlierSlots.push_back(relic);
                coverage.reoffers += contains(sold, relic) ? 1 : 0;
            }
            previousOffers = offers;
            checkBlockedActions(game);
            auto noShopping = game;
            for (const auto& item : catalog) {
                const auto before = game.state();
                const bool expected = contains(offers, item.id) && !game.hasRelic(item.id)
                    && before.owned.size() < 4 && before.coins >= item.cost;
                check(game.buy(item.id) == expected, "buy enforces offers, inventory, funds and ownership");
                if (expected) {
                    ++coverage.purchases;
                    check(game.boughtThisShop(item.id), "shop purchase query records a successful buy");
                    check(game.state().coins == before.coins - item.cost
                        && game.state().owned.size() == before.owned.size() + 1 && game.hasRelic(item.id), "purchase pays listed price for one upgrade");
                    const auto bought = game.state();
                    check(!game.buy(item.id) && sameState(bought, game.state()), "duplicate purchase is harmless");
                } else {
                    check(sameState(before, game.state()), "blocked purchase preserves complete state");
                }
                check(game.state().offers == offers, "transactions do not change current shop offers");
            }
            if (game.state().owned.size() == 4) {
                ++coverage.fullBuilds;
                const auto full = game.state();
                for (auto relic : offers) check(!game.buy(relic) && sameState(full, game.state()), "four active upgrades is a hard limit");
            }
            if (seed % 5 == 0 && !game.state().owned.empty()) {
                const auto relic = game.state().owned.front();
                const auto& item = *std::find_if(catalog.begin(), catalog.end(), [relic](const bj::RelicInfo& info) { return info.id == relic; });
                const auto beforeSale = game.state();
                check(game.sell(relic), "owned upgrade sells in shop");
                ++coverage.sales;
                sold.push_back(relic);
                check(!game.hasRelic(relic) && game.state().coins == beforeSale.coins + item.cost / 2
                    && game.state().owned.size() + 1 == beforeSale.owned.size(), "sale refunds exactly half purchase price");
                const auto afterSale = game.state();
                check(!game.sell(relic) && sameState(afterSale, game.state()), "cannot sell one upgrade twice");
                check(!game.buy(relic) && sameState(afterSale, game.state()), "cannot rebuy a sold upgrade in same shop");
                check(game.boughtThisShop(relic) == contains(offers, relic), "selling retains this visit's purchase marker");
                check(game.state().offers == offers && game.state().usedSecondChance == beforeSale.usedSecondChance,
                    "selling never rerolls offers or recharges shield");
            }
            auto shopping = game;
            shopping.leaveShop();
            noShopping.leaveShop();
            shopping.deal();
            noShopping.deal();
            for (int action = 0; action < 3; ++action) {
                check(sameCards(shopping.state().player, noShopping.state().player)
                    && sameCards(shopping.state().dealer, noShopping.state().dealer), "shop choices do not change next deal or hit cards");
                ++coverage.cardStreams;
                if (shopping.state().phase != bj::Phase::PlayerTurn || noShopping.state().phase != bj::Phase::PlayerTurn) break;
                shopping.hit();
                noShopping.hit();
                if (shopping.state().usedSecondChance != noShopping.state().usedSecondChance) break;
            }
            const auto beforeLeave = game.state();
            check(game.leaveShop(), "shop exit starts next unlimited round");
            const auto afterLeave = game.state();
            for (const auto& item : catalog) check(!game.boughtThisShop(item.id), "shop exit clears purchase markers");
            check(afterLeave.phase == bj::Phase::Ready && afterLeave.round == result.round + 1
                && afterLeave.handsLeft == 5 && afterLeave.roundScore == 0
                && afterLeave.dealerStand == 17 && afterLeave.player.empty() && afterLeave.dealer.empty()
                && afterLeave.outcome == bj::Outcome::None && afterLeave.score.total == 0
                && afterLeave.score.coins == 0 && afterLeave.offers.empty(), "next round resets all five hands, progress and display");
            const std::array<int, 8> goals{30, 40, 50, 60, 70, 80, 90, 100};
            check(afterLeave.goal == goals[static_cast<std::size_t>(std::min(afterLeave.round, 8) - 1)],
                "round goals rise by ten and remain capped at one hundred");
            check(afterLeave.coins == beforeLeave.coins && afterLeave.totalScore == beforeLeave.totalScore
                && afterLeave.wins == beforeLeave.wins && afterLeave.owned == beforeLeave.owned
                && afterLeave.streak == beforeLeave.streak && !afterLeave.usedSecondChance,
                "next round keeps run rewards, upgrades and streak but recharges the once-per-round shield");
            coverage.shieldResets += beforeLeave.usedSecondChance ? 1 : 0;
            check(dealtHand < 499, "seeded run finishes without an artificial round cap");
        }
    }
    check(coverage.wins > 0 && coverage.pushes > 0 && coverage.losses > 0 && coverage.failedRounds == 3000,
        "simulation covers all hand outcomes and finishes every run on an unmet round goal");
    check(coverage.lossContinues > 0 && coverage.pushContinues > 0 && coverage.winContinues > 0,
        "wins below target, pushes and individual losses all continue the same round");
    check(coverage.lastHandClears > 0 && coverage.earlyClears > 0 && coverage.lastHandWinFails > 0,
        "simulation clears goals early and on the final hand, and fails an insufficient final-hand win");
    check(coverage.longRuns > 0, "runs continue beyond five rounds and old three-stage structure");
    check(coverage.soft17 > 0, "soft-17 dealer stands exercised");
    check(coverage.cappedGoals > 0, "longer runs exercise the one-hundred-point target cap");
    check(coverage.streakBoosts > 0 && coverage.streakCaps > 0, "Epic Streak multiplies winning points and remains capped after four earlier wins");
    check(coverage.naturalPrecisionStacks > 0 && coverage.naturalStreakStacks > 0,
        "Blackjack Bonus stacks with Precision and is included before a Streak multiplier");
    check(coverage.purchases > 0 && coverage.sales > 0 && coverage.reoffers > 0 && coverage.fullBuilds > 0,
        "simulation buys, sells, reoffers sold upgrades and reaches four active upgrades");
    check(coverage.cardStreams > 0, "transactions compared against no-shopping runs");
    check(coverage.shields > 0 && coverage.spentShields > 0 && coverage.persistentShields > 0 && coverage.shieldResets > 0,
        "shield rescues one bust, stays spent within a round and recharges at the next round");
    for (const auto& item : catalog) {
        if (item.id != bj::Relic::SecondChance) {
            const auto index = static_cast<std::size_t>(item.id);
            check(coverage.active[index] > 0 && coverage.inactive[index] > 0, std::string(item.name) + " condition checked both met and unmet");
        }
    }
    std::cout << "Seeded runs: 3000; wins " << coverage.wins << ", pushes " << coverage.pushes
              << ", lost hands " << coverage.losses << "; cleared rounds " << coverage.clearedRounds
              << ", final-hand clears " << coverage.lastHandClears << ", maximum round " << coverage.maxRound
              << "; purchases " << coverage.purchases << ", sales " << coverage.sales
              << ", sold reoffers " << coverage.reoffers << ", full builds " << coverage.fullBuilds
              << "; shield saves " << coverage.shields << ", recharges " << coverage.shieldResets << '\n';
}

} // namespace

int main() {
    try {
        testHands();
        testDeterminismAndBlockedRandomness();
        testNaturalBaseAndActualBust();
        testBlackjackBonusCounterfactual();
        testUnbiasedOpeningDistribution();
        testRarityDistribution();
        testSeededRunsAndShop();
    } catch (const std::exception& error) {
        std::cerr << "Unexpected exception: " << error.what() << '\n';
        return 1;
    }
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
