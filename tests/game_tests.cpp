#include "game/Game.hpp"

#include <algorithm>
#include <cmath>
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
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i].rank != b[i].rank || a[i].suit != b[i].suit) {
            return false;
        }
    }
    return true;
}

bool sameState(const bj::Snapshot& a, const bj::Snapshot& b) {
    return sameCards(a.player, b.player) && sameCards(a.dealer, b.dealer)
        && a.phase == b.phase && a.outcome == b.outcome && a.stage == b.stage
        && a.goal == b.goal && a.stageScore == b.stageScore && a.roundsLeft == b.roundsLeft
        && a.coins == b.coins && a.streak == b.streak && a.dealerStand == b.dealerStand
        && a.seed == b.seed && a.usedSecondChance == b.usedSecondChance && a.owned == b.owned
        && a.score.base == b.score.base && a.score.bonus == b.score.bonus
        && a.score.multiplier == b.score.multiplier && a.score.total == b.score.total
        && a.message == b.message;
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
        check(!game.leaveShop(), "leaveShop is restricted to Shop");
    }
    check(sameState(original, game.state()), "blocked actions preserve every snapshot field");
}

void testHands() {
    auto value = bj::evaluateHand({});
    check(value.total == 0 && !value.soft && !value.blackjack && !value.bust, "empty hand");
    value = bj::evaluateHand(hand({1, 13}));
    check(value.total == 21 && value.soft && value.blackjack && !value.bust, "ace + king blackjack");
    value = bj::evaluateHand(hand({1, 6}));
    check(value.total == 17 && value.soft, "soft 17");
    value = bj::evaluateHand(hand({1, 6, 10}));
    check(value.total == 17 && !value.soft, "ace drops from 11 to 1");
    value = bj::evaluateHand(hand({1, 1, 9}));
    check(value.total == 21 && value.soft && !value.blackjack, "three-card 21 is not natural");
    value = bj::evaluateHand(hand({1, 1, 1, 9}));
    check(value.total == 12 && !value.soft && !value.bust, "multiple aces drop together");
    value = bj::evaluateHand(hand({10, 11, 12}));
    check(value.total == 30 && value.bust && !value.soft, "face cards count as 10");
    value = bj::evaluateHand(hand({1, 1, 1, 1, 7}));
    check(value.total == 21 && value.soft, "four aces can retain one soft ace");
    bool rejected = false;
    try {
        bj::evaluateHand(hand({0}));
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected, "invalid rank is rejected");
}

// A separate outcome oracle checks ordering of bust, natural, and ordinary 21.
bj::Outcome expectedOutcome(const bj::Snapshot& state) {
    const auto player = bj::evaluateHand(state.player);
    const auto dealer = bj::evaluateHand(state.dealer);
    if (player.bust) {
        return bj::Outcome::Loss;
    }
    if (dealer.blackjack) {
        return player.blackjack ? bj::Outcome::Push : bj::Outcome::Loss;
    }
    if (player.blackjack || dealer.bust) {
        return bj::Outcome::Win;
    }
    if (player.total == dealer.total) {
        return bj::Outcome::Push;
    }
    return player.total > dealer.total ? bj::Outcome::Win : bj::Outcome::Loss;
}

bool chooseHit(const bj::Snapshot& state, std::uint32_t seed) {
    const auto value = bj::evaluateHand(state.player);
    const int threshold = 16 + static_cast<int>(seed % 4);
    if (state.usedSecondChance && state.message.find("saved you") != std::string::npos) {
        return true; // Exercise a second bust attempt after the shield is spent.
    }
    if (std::find(state.owned.begin(), state.owned.end(), bj::Relic::LongHand) != state.owned.end()
        && state.player.size() < 4 && value.total < 20) {
        return true;
    }
    return value.total < threshold;
}

void buyBuild(bj::Game& game, std::uint32_t seed) {
    for (int offset = 0; offset < 4; ++offset) {
        const auto relic = static_cast<bj::Relic>((seed + static_cast<std::uint32_t>(offset)) % 4);
        game.buy(relic);
    }
}

void testDeterminismAndRestart() {
    bj::Game first(112358);
    bj::Game second(112358);
    const auto initial = first.state();
    for (int action = 0; action < 100; ++action) {
        check(sameState(first.state(), second.state()), "identical seed and actions yield identical state");
        switch (first.state().phase) {
        case bj::Phase::Ready:
            first.deal();
            second.deal();
            break;
        case bj::Phase::PlayerTurn:
            if (chooseHit(first.state(), 112358)) {
                first.hit();
                second.hit();
            } else {
                first.stand();
                second.stand();
            }
            break;
        case bj::Phase::RoundOver:
            first.advance();
            second.advance();
            break;
        case bj::Phase::Shop:
            buyBuild(first, 112358);
            buyBuild(second, 112358);
            first.leaveShop();
            second.leaveShop();
            break;
        case bj::Phase::RunWon:
        case bj::Phase::RunLost:
            action = 100;
            break;
        }
    }
    first.restart(112358);
    second.restart(112358);
    check(sameState(initial, first.state()), "restart clears all progress and relics");
    first.deal();
    second.deal();
    check(sameState(first.state(), second.state()), "restart also resets the random generator");
    bj::Game other(112359);
    other.deal();
    check(!sameCards(first.state().player, other.state().player)
        || !sameCards(first.state().dealer, other.state().dealer), "different seeds change a shuffled opening");
}

struct Coverage {
    int wins = 0;
    int losses = 0;
    int pushes = 0;
    int naturalWins = 0;
    int naturalPushes = 0;
    int playerBusts = 0;
    int dealerBusts = 0;
    int precision = 0;
    int longHand = 0;
    int streak = 0;
    int shields = 0;
    int secondBusts = 0;
    int resetShields = 0;
    int soft17 = 0;
    int boss = 0;
    int runWins = 0;
    int runLosses = 0;
    int lastHandClear = 0;
};

void testSeededRuns() {
    Coverage coverage;
    const auto& catalog = bj::relicCatalog();
    check(catalog.size() == 4, "shop exposes the four documented relics");
    for (const auto& item : catalog) {
        check(item.cost == 3 && item.name[0] != '\0' && item.description[0] != '\0', "catalog entries have cost and text");
    }

    for (std::uint32_t seed = 0; seed < 3000; ++seed) {
        bj::Game game(seed);
        check(game.state().phase == bj::Phase::Ready && game.state().stage == 1
            && game.state().roundsLeft == 5 && game.state().goal == 30
            && game.state().coins == 0 && game.state().dealerStand == 17, "new run defaults");

        for (int round = 0; round < 15; ++round) {
            checkBlockedActions(game);
            const auto beforeRound = game.state();
            check(game.deal(), "deal succeeds in Ready");
            check(game.state().roundsLeft == beforeRound.roundsLeft - 1, "each deal spends exactly one hand");
            check(game.state().player.size() == 2 && game.state().dealer.size() == 2, "opening deal has two cards each");

            for (int action = 0; game.state().phase == bj::Phase::PlayerTurn && action < 52; ++action) {
                checkBlockedActions(game);
                const auto beforeAction = game.state();
                if (chooseHit(beforeAction, seed)) {
                    check(game.hit(), "hit succeeds during player turn");
                    if (!beforeAction.usedSecondChance && game.state().usedSecondChance) {
                        ++coverage.shields;
                        check(game.hasRelic(bj::Relic::SecondChance), "only Second Chance can consume a shield");
                        check(game.state().phase == bj::Phase::PlayerTurn, "shield keeps player turn active");
                        check(sameCards(beforeAction.player, game.state().player), "shield restores the exact previous hand");
                        check(game.state().stageScore == beforeAction.stageScore, "shield does not score a hand");
                    }
                    if (beforeAction.usedSecondChance && game.state().phase == bj::Phase::RoundOver
                        && bj::evaluateHand(game.state().player).bust) {
                        ++coverage.secondBusts;
                        check(game.state().outcome == bj::Outcome::Loss, "spent shield cannot rescue another bust");
                    }
                } else {
                    check(game.stand(), "stand succeeds during player turn");
                }
                check(game.state().coins == beforeAction.coins, "playing cards never changes coins");
            }

            const auto result = game.state();
            check(result.phase == bj::Phase::RoundOver, "every dealt hand finishes");
            check(result.outcome == expectedOutcome(result), "outcome follows blackjack comparison rules");
            const auto player = bj::evaluateHand(result.player);
            const auto dealer = bj::evaluateHand(result.dealer);
            bool seen[52] = {};
            for (const auto* cards : {&result.player, &result.dealer}) {
                for (const auto& card : *cards) {
                    const auto id = static_cast<int>(card.suit) * 13 + card.rank - 1;
                    check(id >= 0 && id < 52, "all dealt cards belong to a standard deck");
                    if (id >= 0 && id < 52) {
                        check(!seen[id], "a card cannot appear twice within a round");
                        seen[id] = true;
                    }
                }
            }
            if (!player.bust && !player.blackjack && !dealer.blackjack) {
                check(dealer.total >= result.dealerStand, "dealer draws to the stage threshold");
                if (result.dealer.size() > 2) {
                    auto previous = result.dealer;
                    previous.pop_back();
                    check(bj::evaluateHand(previous).total < result.dealerStand, "dealer never takes an extra card after reaching threshold");
                }
                if (result.dealerStand == 17 && dealer.total == 17 && dealer.soft) {
                    ++coverage.soft17;
                }
            }
            coverage.playerBusts += player.bust ? 1 : 0;
            coverage.dealerBusts += dealer.bust ? 1 : 0;
            coverage.boss += result.stage == 3 ? 1 : 0;

            if (result.outcome == bj::Outcome::Win) {
                ++coverage.wins;
                int bonus = player.blackjack ? 10 : 0;
                float multiplier = 1.0f;
                if (game.hasRelic(bj::Relic::LongHand) && result.player.size() >= 4) {
                    bonus += 30;
                    ++coverage.longHand;
                }
                if (game.hasRelic(bj::Relic::Streak)) {
                    multiplier += 0.5f * static_cast<float>(beforeRound.streak);
                    coverage.streak += beforeRound.streak > 0 ? 1 : 0;
                }
                if (game.hasRelic(bj::Relic::Precision) && player.total == 21) {
                    multiplier *= 2.0f;
                    ++coverage.precision;
                }
                const auto expectedPoints = static_cast<int>(std::lround((player.total + bonus) * multiplier));
                check(result.score.base == player.total && result.score.bonus == bonus
                    && result.score.multiplier == multiplier && result.score.total == expectedPoints, "score breakdown includes all owned effects");
                check(result.stageScore == beforeRound.stageScore + expectedPoints, "winning points accumulate once");
                check(result.streak == beforeRound.streak + 1, "win increases streak after scoring");
                coverage.naturalWins += player.blackjack ? 1 : 0;
            } else {
                check(result.score.total == 0 && result.score.base == 0 && result.score.bonus == 0
                    && result.score.multiplier == 1.0f, "non-wins have an empty score breakdown");
                check(result.stageScore == beforeRound.stageScore, "loss and push earn no points");
                if (result.outcome == bj::Outcome::Loss) {
                    ++coverage.losses;
                    check(result.streak == 0, "loss resets streak");
                } else {
                    ++coverage.pushes;
                    coverage.naturalPushes += player.blackjack && dealer.blackjack ? 1 : 0;
                    check(result.streak == beforeRound.streak, "push preserves streak");
                }
            }

            checkBlockedActions(game);
            check(game.advance(), "advance succeeds after a hand");
            if (result.stageScore >= result.goal) {
                check(game.state().coins == result.coins + 5, "clearing an ante awards exactly five coins");
                coverage.lastHandClear += result.roundsLeft == 0 ? 1 : 0;
                check(game.state().phase == (result.stage == 3 ? bj::Phase::RunWon : bj::Phase::Shop), "completed target has priority over exhausted hands");
            } else if (result.roundsLeft == 0) {
                check(game.state().phase == bj::Phase::RunLost, "missing target after fifth hand ends the run");
                check(game.state().coins == result.coins, "failed ante awards no coins");
            } else {
                check(game.state().phase == bj::Phase::Ready, "unfinished ante returns to Ready");
            }

            if (game.state().phase == bj::Phase::Shop) {
                checkBlockedActions(game);
                auto beforeBuy = game.state();
                check(!game.buy(static_cast<bj::Relic>(99)), "unknown relic cannot be purchased");
                check(sameState(beforeBuy, game.state()), "invalid purchase leaves all state intact");
                for (int offset = 0; offset < 4; ++offset) {
                    const auto relic = static_cast<bj::Relic>((seed + static_cast<std::uint32_t>(offset)) % 4);
                    beforeBuy = game.state();
                    const bool expected = !game.hasRelic(relic) && beforeBuy.coins >= 3;
                    check(game.buy(relic) == expected, "shop enforces ownership and available coins");
                    if (expected) {
                        check(game.hasRelic(relic) && game.state().coins == beforeBuy.coins - 3
                            && game.state().owned.size() == beforeBuy.owned.size() + 1, "purchase deducts cost and adds one relic");
                        const auto bought = game.state();
                        check(!game.buy(relic) && sameState(bought, game.state()), "duplicate purchase is harmless");
                    } else {
                        check(sameState(beforeBuy, game.state()), "unaffordable purchase is harmless");
                    }
                }
                const auto beforeLeave = game.state();
                check(game.leaveShop(), "leaveShop starts the next stage");
                const auto afterLeave = game.state();
                coverage.resetShields += beforeLeave.usedSecondChance ? 1 : 0;
                check(afterLeave.phase == bj::Phase::Ready && afterLeave.stage == result.stage + 1
                    && afterLeave.goal == (afterLeave.stage == 2 ? 65 : 100)
                    && afterLeave.stageScore == 0 && afterLeave.roundsLeft == 5
                    && !afterLeave.usedSecondChance, "next stage resets target progress, hands, and shield");
                check(afterLeave.dealerStand == (afterLeave.stage == 3 ? 18 : 17), "final boss stands at 18");
                check(afterLeave.coins == beforeLeave.coins && afterLeave.owned == beforeLeave.owned
                    && afterLeave.streak == beforeLeave.streak, "coins, relics, and streak carry to next stage");
                check(afterLeave.player.empty() && afterLeave.dealer.empty()
                    && afterLeave.outcome == bj::Outcome::None && afterLeave.score.total == 0, "next stage clears previous hand display");
            }
            if (game.state().phase == bj::Phase::RunWon || game.state().phase == bj::Phase::RunLost) {
                coverage.runWins += game.state().phase == bj::Phase::RunWon ? 1 : 0;
                coverage.runLosses += game.state().phase == bj::Phase::RunLost ? 1 : 0;
                checkBlockedActions(game);
                const bj::Game fresh(seed);
                game.restart(seed);
                check(sameState(fresh.state(), game.state()), "restart after terminal state produces a clean run");
                break;
            }
        }
    }

    check(coverage.wins > 0 && coverage.losses > 0 && coverage.pushes > 0, "simulation covers every outcome");
    check(coverage.naturalWins > 0 && coverage.naturalPushes > 0, "simulation covers natural blackjack wins and ties");
    check(coverage.playerBusts > 0 && coverage.dealerBusts > 0, "simulation covers both bust paths");
    check(coverage.precision > 0 && coverage.longHand > 0 && coverage.streak > 0, "simulation exercises every scoring relic");
    check(coverage.shields > 0 && coverage.secondBusts > 0 && coverage.resetShields > 0, "simulation exercises shield save, spent shield, and stage refresh");
    check(coverage.soft17 > 0 && coverage.boss > 0, "simulation exercises soft-17 stands and the final boss");
    check(coverage.runWins > 0 && coverage.runLosses > 0 && coverage.lastHandClear > 0, "simulation reaches both endings and clears a target on the last hand");
    std::cout << "Seeded runs: " << coverage.runWins << " won, " << coverage.runLosses
              << " lost; shield saves: " << coverage.shields << "; scoring relic activations: "
              << coverage.precision + coverage.longHand + coverage.streak << '\n';
}

} // namespace

int main() {
    try {
        testHands();
        testDeterminismAndRestart();
        testSeededRuns();
    } catch (const std::exception& error) {
        std::cerr << "Unexpected exception: " << error.what() << '\n';
        return 1;
    }
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
