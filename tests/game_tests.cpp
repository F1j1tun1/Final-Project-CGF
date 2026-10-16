#include "game/Game.hpp"

#include <algorithm>
#include <array>
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
    if (game.state().phase == bj::Phase::RoundOver) return game;
    bj::Game best = game;
    int bestRank = -1;
    for (int action = 0; action < 52; ++action) {
        if (game.state().phase == bj::Phase::RoundOver) {
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

struct Coverage {
    int wins = 0, pushes = 0, losses = 0, soft17 = 0, longRuns = 0;
    int purchases = 0, sales = 0, reoffers = 0, fullBuilds = 0, cardStreams = 0;
    int shields = 0, spentShields = 0, persistentShields = 0;
    std::array<int, 12> active{};
    std::array<int, 12> inactive{};
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
        check(result.coins == before.coins && result.totalScore == before.totalScore
            && result.wins == before.wins, "non-wins leave accumulated rewards unchanged");
        check(result.streak == (result.outcome == bj::Outcome::Loss ? 0 : before.streak), "loss breaks streak; push preserves it");
        return;
    }
    int expectedCoins = 3;
    int expectedBonus = player.blackjack ? 10 : 0;
    for (auto relic : before.owned) {
        bool active = false;
        int bonus = 0;
        switch (relic) {
        case bj::Relic::Precision: active = player.total == 21; break;
        case bj::Relic::LongHand: active = result.player.size() >= 4; break;
        case bj::Relic::Streak: active = (before.streak + 1) % 3 == 0; break;
        case bj::Relic::RoyalPurse: active = player.blackjack; break;
        case bj::Relic::SoftTouch: active = player.soft; break;
        case bj::Relic::NarrowEscape: active = dealer.bust; break;
        case bj::Relic::RedRibbon: active = red(result.player[0]) && red(result.player[1]); break;
        case bj::Relic::BlackVelvet: active = !red(result.player[0]) && !red(result.player[1]); break;
        case bj::Relic::EvenOdds: active = player.total % 2 == 0; break;
        case bj::Relic::CourtFavor:
            active = std::any_of(result.player.begin(), result.player.end(), [](const bj::Card& card) { return card.rank > 10; });
            bonus = 3;
            break;
        case bj::Relic::HumbleCrown: active = player.total <= 18; bonus = 5; break;
        case bj::Relic::SecondChance: continue;
        }
        const auto index = static_cast<std::size_t>(relic);
        ++(active ? coverage.active[index] : coverage.inactive[index]);
        if (active) {
            expectedCoins += bonus == 0 ? 1 : 0;
            expectedBonus += bonus;
        }
    }
    check(result.score.base == player.total && result.score.bonus == expectedBonus
        && result.score.total == player.total + expectedBonus && result.score.multiplier == 1.0f,
        "points match the natural and conditional score upgrades");
    check(result.score.coins == expectedCoins && result.coins == before.coins + expectedCoins,
        "win income is three coins plus eligible modest bonuses");
    check(result.totalScore == before.totalScore + result.score.total && result.wins == before.wins + 1,
        "winning points and wins accumulate exactly once");
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
        if (before.usedSecondChance && game.state().phase == bj::Phase::RoundOver
            && bj::evaluateHand(game.state().player).bust) {
            ++coverage.spentShields;
            check(game.state().outcome == bj::Outcome::Loss, "spent shield cannot rescue another bust");
        }
    }
}

void testSeededRunsAndShop() {
    Coverage coverage;
    const auto& catalog = bj::relicCatalog();
    check(catalog.size() == 12, "catalogue has twelve choices");
    for (const auto& item : catalog) {
        check(item.cost > 0 && item.cost <= 8 && item.cost % 2 == 0
            && item.name[0] != '\0' && item.description[0] != '\0', "bounded even prices and descriptive text");
        check(std::count_if(catalog.begin(), catalog.end(), [&](const bj::RelicInfo& other) { return item.id == other.id; }) == 1,
            "unique catalogue identifiers");
    }
    for (std::uint32_t seed = 0; seed < 3000; ++seed) {
        bj::Game game(seed);
        check(game.state().phase == bj::Phase::Ready && game.state().round == 1
            && game.state().coins == 0 && game.state().wins == 0 && game.state().totalScore == 0
            && game.state().dealerStand == 17 && game.state().owned.empty() && game.state().offers.empty(), "new run defaults");
        std::vector<bj::Relic> previousOffers;
        std::vector<bj::Relic> sold;
        for (int round = 0; round < 100; ++round) {
            checkBlockedActions(game);
            const auto beforeRound = game.state();
            check(game.deal(), "deal succeeds in Ready");
            check(game.state().player.size() == 2 && game.state().dealer.size() == 2, "opening deal gives two cards to each side");
            check(game.state().usedSecondChance == beforeRound.usedSecondChance, "deal does not recharge shield");
            probeShield(game, coverage);
            game = finishBest(game);
            const auto result = game.state();
            check(result.phase == bj::Phase::RoundOver && result.outcome == expectedOutcome(result), "hand resolves by blackjack comparison");
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
            checkBlockedActions(game);
            check(game.advance(), "advance succeeds after a round");
            check(game.state().coins == result.coins && game.state().totalScore == result.totalScore, "advance never awards the same round twice");
            if (result.outcome == bj::Outcome::Loss) {
                check(game.state().phase == bj::Phase::RunLost, "first defeat ends the run");
                checkBlockedActions(game);
                game.restart(seed);
                const bj::Game fresh(seed);
                check(sameState(fresh.state(), game.state()), "restart after defeat clears full run state");
                break;
            }
            check(game.state().phase == bj::Phase::Shop, "win and push both lead to a shop");
            const auto offers = game.state().offers;
            check(offers.size() == 2 && offers[0] != offers[1], "shop offers exactly two distinct upgrades");
            for (auto relic : offers) {
                check(!game.hasRelic(relic), "shops do not offer an active upgrade");
                check(!contains(previousOffers, relic), "next shop avoids both preceding offers");
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
                && afterLeave.dealerStand == 17 && afterLeave.player.empty() && afterLeave.dealer.empty()
                && afterLeave.outcome == bj::Outcome::None && afterLeave.score.total == 0
                && afterLeave.score.coins == 0 && afterLeave.offers.empty(), "next round clears hand and shop display");
            check(afterLeave.coins == beforeLeave.coins && afterLeave.totalScore == beforeLeave.totalScore
                && afterLeave.wins == beforeLeave.wins && afterLeave.owned == beforeLeave.owned
                && afterLeave.streak == beforeLeave.streak && afterLeave.usedSecondChance == beforeLeave.usedSecondChance,
                "next round retains rewards, upgrades, streak and shield use");
            coverage.persistentShields += afterLeave.usedSecondChance ? 1 : 0;
            check(round < 99, "seeded run reaches first defeat without an artificial stage cap");
        }
    }
    check(coverage.wins > 0 && coverage.pushes > 0 && coverage.losses == 3000, "simulation covers all outcomes and ends each run at first defeat");
    check(coverage.longRuns > 0, "runs continue beyond five rounds and old three-stage structure");
    check(coverage.soft17 > 0, "soft-17 dealer stands exercised");
    check(coverage.purchases > 0 && coverage.sales > 0 && coverage.reoffers > 0 && coverage.fullBuilds > 0,
        "simulation buys, sells, reoffers sold upgrades and reaches four active upgrades");
    check(coverage.cardStreams > 0, "transactions compared against no-shopping runs");
    check(coverage.shields > 0 && coverage.spentShields > 0 && coverage.persistentShields > 0,
        "shield rescues a bust and stays spent across hands");
    for (const auto& item : catalog) {
        if (item.id != bj::Relic::SecondChance) {
            const auto index = static_cast<std::size_t>(item.id);
            check(coverage.active[index] > 0 && coverage.inactive[index] > 0, std::string(item.name) + " condition checked both met and unmet");
        }
    }
    std::cout << "Seeded runs: 3000; wins " << coverage.wins << ", pushes " << coverage.pushes
              << "; purchases " << coverage.purchases << ", sales " << coverage.sales
              << ", sold reoffers " << coverage.reoffers << ", full builds " << coverage.fullBuilds
              << "; shield saves " << coverage.shields << '\n';
}

} // namespace

int main() {
    try {
        testHands();
        testDeterminismAndBlockedRandomness();
        testUnbiasedOpeningDistribution();
        testSeededRunsAndShop();
    } catch (const std::exception& error) {
        std::cerr << "Unexpected exception: " << error.what() << '\n';
        return 1;
    }
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
