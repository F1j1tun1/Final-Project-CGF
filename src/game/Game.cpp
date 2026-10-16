#include "Game.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace bj {
namespace {

constexpr int kWinCoins = 3;
constexpr int kHandsPerRound = 5;
constexpr std::size_t kMaxRelics = 4;

int goalForRound(int round) {
    // Evaluate the multiplication only while the result remains below the cap.
    return round >= 8 ? 100 : 30 + 10 * (round - 1);
}

// Rejection sampling keeps seeded shuffles identical across standard libraries.
std::uint32_t boundedRandom(std::mt19937& random, std::uint32_t bound) {
    const auto threshold = (std::uint32_t{0} - bound) % bound;
    std::uint32_t value;
    do {
        value = static_cast<std::uint32_t>(random());
    } while (value < threshold);
    return value % bound;
}

bool isRed(const Card& card) {
    return card.suit == Suit::Diamonds || card.suit == Suit::Hearts;
}

const RelicInfo* relicInfo(Relic relic) {
    const auto& catalog = relicCatalog();
    const auto item = std::find_if(catalog.begin(), catalog.end(), [relic](const RelicInfo& info) {
        return info.id == relic;
    });
    return item == catalog.end() ? nullptr : &*item;
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
        {Relic::Precision, "Precision", "Win with 21 for +1 coin.", 6},
        {Relic::LongHand, "Long Hand", "Win with 4+ cards for +1 coin.", 6},
        {Relic::SecondChance, "Second Chance", "Discard your first bust card. Once per round.", 8},
        {Relic::Streak, "Streak", "Every third consecutive win earns +1 coin.", 6},
        {Relic::RoyalPurse, "Royal Purse", "Win with a natural blackjack for +1 coin.", 2},
        {Relic::SoftTouch, "Soft Touch", "Win with a soft hand for +1 coin.", 2},
        {Relic::NarrowEscape, "Narrow Escape", "Win when the dealer busts for +1 coin.", 4},
        {Relic::RedRibbon, "Red Ribbon", "Win with two red opening cards for +1 coin.", 4},
        {Relic::BlackVelvet, "Black Velvet", "Win with two black opening cards for +1 coin.", 4},
        {Relic::EvenOdds, "Even Odds", "Win with an even hand total for +1 coin.", 4},
        {Relic::CourtFavor, "Court Favor", "Win with at least one J, Q or K for +3 points.", 2},
        {Relic::HumbleCrown, "Humble Crown", "Win with 18 or less for +5 points.", 2}
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
    state_.message = "Five hands to reach the round goal. Each win earns points and 3 coins.";
    cardRandom_.seed(seed);
    shopRandom_.seed(seed ^ 0x9e3779b9u);
    deck_.clear();
    previousOffers_.clear();
    boughtThisShop_.clear();
}

bool Game::hasRelic(Relic relic) const {
    return std::find(state_.owned.begin(), state_.owned.end(), relic) != state_.owned.end();
}

bool Game::boughtThisShop(Relic relic) const {
    return std::find(boughtThisShop_.begin(), boughtThisShop_.end(), relic) != boughtThisShop_.end();
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
        const auto j = boundedRandom(cardRandom_, static_cast<std::uint32_t>(i + 1));
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
    if (state_.phase != Phase::Ready || state_.handsLeft <= 0) {
        return false;
    }
    shuffleDeck();
    state_.player.clear();
    state_.dealer.clear();
    state_.score = ScoreBreakdown{};
    state_.outcome = Outcome::None;
    --state_.handsLeft;
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
        state_.score.coins = kWinCoins;
        if (hasRelic(Relic::LongHand) && state_.player.size() >= 4) {
            ++state_.score.coins;
        }
        if (hasRelic(Relic::Streak) && (state_.streak + 1) % 3 == 0) {
            ++state_.score.coins;
        }
        if (hasRelic(Relic::Precision) && player.total == 21) {
            ++state_.score.coins;
        }
        if (hasRelic(Relic::RoyalPurse) && player.blackjack) {
            ++state_.score.coins;
        }
        if (hasRelic(Relic::SoftTouch) && player.soft) {
            ++state_.score.coins;
        }
        if (hasRelic(Relic::NarrowEscape) && dealer.bust) {
            ++state_.score.coins;
        }
        const bool firstRed = isRed(state_.player[0]);
        const bool secondRed = isRed(state_.player[1]);
        if (hasRelic(Relic::RedRibbon) && firstRed && secondRed) {
            ++state_.score.coins;
        }
        if (hasRelic(Relic::BlackVelvet) && !firstRed && !secondRed) {
            ++state_.score.coins;
        }
        if (hasRelic(Relic::EvenOdds) && player.total % 2 == 0) {
            ++state_.score.coins;
        }
        if (hasRelic(Relic::CourtFavor) && std::any_of(state_.player.begin(), state_.player.end(), [](const Card& card) {
                return card.rank >= 11;
            })) {
            state_.score.bonus += 3;
        }
        if (hasRelic(Relic::HumbleCrown) && player.total <= 18) {
            state_.score.bonus += 5;
        }
        state_.score.total = state_.score.base + state_.score.bonus;
        state_.roundScore += state_.score.total;
        state_.totalScore += state_.score.total;
        state_.coins += state_.score.coins;
        ++state_.wins;
        ++state_.streak;
        state_.message = player.blackjack ? "Blackjack! Coins and bonus points earned." : "You win! Coins and points earned.";
    } else if (state_.outcome == Outcome::Loss) {
        state_.streak = 0;
        state_.message = player.bust ? "Bust! This hand earns no points." : "Dealer wins this hand. No points earned.";
    } else {
        state_.message = "Push. No reward; your streak stays alive.";
    }
    state_.phase = Phase::RoundOver;
}

bool Game::advance() {
    if (state_.phase != Phase::RoundOver) {
        return false;
    }
    if (state_.roundScore >= state_.goal) {
        openShop();
    } else if (state_.handsLeft == 0) {
        state_.phase = Phase::RunLost;
        state_.message = "Out of hands before the goal. Start again and try another build.";
    } else {
        state_.phase = Phase::Ready;
        state_.message = "Keep playing. Reach the round goal before your five hands run out.";
    }
    return true;
}

bool Game::buy(Relic relic) {
    if (state_.phase != Phase::Shop || hasRelic(relic) || state_.owned.size() >= kMaxRelics
        || std::find(state_.offers.begin(), state_.offers.end(), relic) == state_.offers.end()
        || boughtThisShop(relic)) {
        return false;
    }
    const auto* item = relicInfo(relic);
    if (!item || state_.coins < item->cost) {
        return false;
    }
    state_.coins -= item->cost;
    state_.owned.push_back(relic);
    boughtThisShop_.push_back(relic);
    state_.message = std::string(item->name) + " purchased. Your build is growing.";
    return true;
}

bool Game::sell(Relic relic) {
    if (state_.phase != Phase::Shop || !hasRelic(relic)) {
        return false;
    }
    const auto* item = relicInfo(relic);
    if (!item) {
        return false;
    }
    state_.owned.erase(std::find(state_.owned.begin(), state_.owned.end(), relic));
    state_.coins += item->cost / 2;
    state_.message = std::string(item->name) + " sold for " + std::to_string(item->cost / 2) + " coins.";
    return true;
}

void Game::openShop() {
    std::vector<Relic> candidates;
    std::vector<Relic> repeats;
    for (const auto& item : relicCatalog()) {
        if (hasRelic(item.id)) {
            continue;
        }
        const bool previous = std::find(previousOffers_.begin(), previousOffers_.end(), item.id) != previousOffers_.end();
        (previous ? repeats : candidates).push_back(item.id);
    }
    // Prefer fresh offers. Reuse an earlier offer only if the eligible pool is too small.
    if (candidates.size() < 2) {
        candidates.insert(candidates.end(), repeats.begin(), repeats.end());
    }
    for (std::size_t i = candidates.size(); i > 1; --i) {
        const auto j = boundedRandom(shopRandom_, static_cast<std::uint32_t>(i));
        std::swap(candidates[i - 1], candidates[j]);
    }
    state_.offers.assign(candidates.begin(), candidates.begin() + std::min<std::size_t>(2, candidates.size()));
    previousOffers_ = state_.offers;
    boughtThisShop_.clear();
    state_.phase = Phase::Shop;
    state_.message = "Round cleared! Choose an upgrade, sell an owned upgrade, or continue.";
}

bool Game::leaveShop() {
    if (state_.phase != Phase::Shop) {
        return false;
    }
    if (state_.round < std::numeric_limits<int>::max()) {
        ++state_.round;
    }
    state_.handsLeft = kHandsPerRound;
    state_.roundScore = 0;
    state_.goal = goalForRound(state_.round);
    state_.usedSecondChance = false;
    state_.player.clear();
    state_.dealer.clear();
    state_.score = ScoreBreakdown{};
    state_.outcome = Outcome::None;
    state_.offers.clear();
    boughtThisShop_.clear();
    state_.phase = Phase::Ready;
    state_.message = "New round. Five hands to reach the next score goal.";
    return true;
}

} // namespace bj
