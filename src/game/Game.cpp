#include "Game.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace bj {
namespace {

constexpr int kHandsPerStage = 5;
constexpr int kGoals[] = {30, 65, 100};
constexpr int kStageReward = 5;

// Rejection sampling keeps seeded shuffles identical across standard libraries.
std::uint32_t boundedRandom(std::mt19937& random, std::uint32_t bound) {
    const auto threshold = (std::uint32_t{0} - bound) % bound;
    std::uint32_t value;
    do {
        value = static_cast<std::uint32_t>(random());
    } while (value < threshold);
    return value % bound;
}

} // namespace

HandValue evaluateHand(const std::vector<Card>& hand) {
    HandValue value;
    int aces = 0;
    for (const auto& card : hand) {
        if (card.rank < 1 || card.rank > 13) {
            throw std::invalid_argument("Card rank must be between 1 and 13");
        }
        value.total += std::min(card.rank, 10);
        if (card.rank == 1) {
            ++aces;
        }
    }
    if (aces > 0 && value.total + 10 <= 21) {
        value.total += 10;
        value.soft = true;
    }
    value.blackjack = hand.size() == 2 && value.total == 21;
    value.bust = value.total > 21;
    return value;
}

const std::vector<RelicInfo>& relicCatalog() {
    static const std::vector<RelicInfo> catalog = {
        {Relic::Precision, "Precision", "A winning 21 earns x2 points.", 3},
        {Relic::LongHand, "Long Hand", "Win with 4+ cards for +30 bonus points.", 3},
        {Relic::SecondChance, "Second Chance", "Discard the first bust card once per stage.", 3},
        {Relic::Streak, "Streak", "Gain +0.5 multiplier per earlier consecutive win.", 3}
    };
    return catalog;
}

Game::Game(std::uint32_t seed) {
    restart(seed);
}

const Snapshot& Game::state() const {
    return state_;
}

void Game::restart(std::uint32_t seed) {
    state_ = Snapshot{};
    state_.seed = seed;
    state_.goal = kGoals[0];
    state_.message = "Deal a hand. Beat the dealer to earn points.";
    random_.seed(seed);
    deck_.clear();
}

bool Game::hasRelic(Relic relic) const {
    return std::find(state_.owned.begin(), state_.owned.end(), relic) != state_.owned.end();
}

void Game::shuffleDeck() {
    deck_.clear();
    deck_.reserve(52);
    for (int suit = 0; suit < 4; ++suit) {
        for (int rank = 1; rank <= 13; ++rank) {
            deck_.push_back({rank, static_cast<Suit>(suit)});
        }
    }
    for (std::size_t i = deck_.size() - 1; i > 0; --i) {
        const auto j = boundedRandom(random_, static_cast<std::uint32_t>(i + 1));
        std::swap(deck_[i], deck_[j]);
    }
}

Card Game::draw() {
    if (deck_.empty()) {
        throw std::logic_error("The round deck is empty");
    }
    Card card = deck_.back();
    deck_.pop_back();
    return card;
}

bool Game::deal() {
    if (state_.phase != Phase::Ready || state_.roundsLeft <= 0) {
        return false;
    }
    shuffleDeck();
    state_.player.clear();
    state_.dealer.clear();
    state_.score = ScoreBreakdown{};
    state_.outcome = Outcome::None;
    --state_.roundsLeft;
    state_.phase = Phase::PlayerTurn;
    state_.player.push_back(draw());
    state_.dealer.push_back(draw());
    state_.player.push_back(draw());
    state_.dealer.push_back(draw());
    state_.message = "Hit to draw a card, or stand to let the dealer play.";
    if (evaluateHand(state_.player).blackjack || evaluateHand(state_.dealer).blackjack) {
        finishRound();
    }
    return true;
}

bool Game::hit() {
    if (state_.phase != Phase::PlayerTurn) {
        return false;
    }
    state_.player.push_back(draw());
    const auto value = evaluateHand(state_.player);
    if (value.bust) {
        if (hasRelic(Relic::SecondChance) && !state_.usedSecondChance) {
            state_.player.pop_back();
            state_.usedSecondChance = true;
            state_.message = "Second Chance saved you! The bust card was discarded.";
        } else {
            finishRound();
        }
    } else if (value.total == 21) {
        stand();
    } else {
        state_.message = "Draw again, or stand with your current hand.";
    }
    return true;
}

bool Game::stand() {
    if (state_.phase != Phase::PlayerTurn) {
        return false;
    }
    while (evaluateHand(state_.dealer).total < state_.dealerStand) {
        state_.dealer.push_back(draw());
    }
    finishRound();
    return true;
}

void Game::finishRound() {
    const auto player = evaluateHand(state_.player);
    const auto dealer = evaluateHand(state_.dealer);
    if (player.bust) {
        state_.outcome = Outcome::Loss;
    } else if (player.blackjack || dealer.blackjack) {
        state_.outcome = player.blackjack == dealer.blackjack
            ? Outcome::Push : (player.blackjack ? Outcome::Win : Outcome::Loss);
    } else if (dealer.bust || player.total > dealer.total) {
        state_.outcome = Outcome::Win;
    } else if (player.total == dealer.total) {
        state_.outcome = Outcome::Push;
    } else {
        state_.outcome = Outcome::Loss;
    }

    state_.score = ScoreBreakdown{};
    if (state_.outcome == Outcome::Win) {
        state_.score.base = player.total;
        if (player.blackjack) {
            state_.score.bonus += 10;
        }
        if (hasRelic(Relic::LongHand) && state_.player.size() >= 4) {
            state_.score.bonus += 30;
        }
        if (hasRelic(Relic::Streak)) {
            state_.score.multiplier += 0.5f * static_cast<float>(state_.streak);
        }
        if (hasRelic(Relic::Precision) && player.total == 21) {
            state_.score.multiplier *= 2.0f;
        }
        state_.score.total = static_cast<int>(std::lround(
            static_cast<float>(state_.score.base + state_.score.bonus) * state_.score.multiplier));
        state_.stageScore += state_.score.total;
        ++state_.streak;
        state_.message = player.blackjack ? "Blackjack! Bonus points earned." : "You win! Points added.";
    } else if (state_.outcome == Outcome::Loss) {
        state_.streak = 0;
        state_.message = player.bust ? "Bust! This hand earns no points." : "Dealer wins. This hand earns no points.";
    } else {
        state_.message = "Push. No points, and your streak stays alive.";
    }
    state_.phase = Phase::RoundOver;
}

bool Game::advance() {
    if (state_.phase != Phase::RoundOver) {
        return false;
    }
    if (state_.stageScore >= state_.goal) {
        state_.coins += kStageReward;
        if (state_.stage == 3) {
            state_.phase = Phase::RunWon;
            state_.message = "Run complete! You beat the final dealer.";
        } else {
            state_.phase = Phase::Shop;
            state_.message = "Stage cleared! Spend your coins on a relic.";
        }
    } else if (state_.roundsLeft == 0) {
        state_.phase = Phase::RunLost;
        state_.message = "Out of hands. Start a new run and try another build.";
    } else {
        state_.phase = Phase::Ready;
        state_.message = "Deal your next hand.";
    }
    return true;
}

bool Game::buy(Relic relic) {
    if (state_.phase != Phase::Shop || hasRelic(relic) || state_.owned.size() >= 4) {
        return false;
    }
    const auto& catalog = relicCatalog();
    const auto item = std::find_if(catalog.begin(), catalog.end(), [relic](const RelicInfo& info) {
        return info.id == relic;
    });
    if (item == catalog.end() || state_.coins < item->cost) {
        return false;
    }
    state_.coins -= item->cost;
    state_.owned.push_back(relic);
    state_.message = std::string(item->name) + " purchased. Your build is growing.";
    return true;
}

bool Game::leaveShop() {
    if (state_.phase != Phase::Shop) {
        return false;
    }
    ++state_.stage;
    state_.goal = kGoals[state_.stage - 1];
    state_.stageScore = 0;
    state_.roundsLeft = kHandsPerStage;
    state_.dealerStand = state_.stage == 3 ? 18 : 17;
    state_.usedSecondChance = false;
    state_.player.clear();
    state_.dealer.clear();
    state_.score = ScoreBreakdown{};
    state_.outcome = Outcome::None;
    state_.phase = Phase::Ready;
    state_.message = state_.stage == 3
        ? "Final stage! The dealer now stands at 18." : "New stage. Five hands to reach the target.";
    return true;
}

} // namespace bj
