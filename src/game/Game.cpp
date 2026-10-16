#include "Game.hpp"

#include <algorithm>
#include <array>
#include <cmath>
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

const char* rarityName(Rarity rarity) {
    switch (rarity) {
    case Rarity::Common: return "Common";
    case Rarity::Uncommon: return "Uncommon";
    case Rarity::Rare: return "Rare";
    case Rarity::Epic: return "Epic";
    case Rarity::Legendary: return "Legendary";
    }
    return "Unknown";
}

int rarityWeight(Rarity rarity) {
    switch (rarity) {
    case Rarity::Common: return 50;
    case Rarity::Uncommon: return 28;
    case Rarity::Rare: return 14;
    case Rarity::Epic: return 6;
    case Rarity::Legendary: return 2;
    }
    return 0;
}

const std::vector<RelicInfo>& relicCatalog() {
    static const std::vector<RelicInfo> catalog = {
        {Relic::Precision, "Precision", "Win with 21 for +1 coin and +5 points.", 6, Rarity::Rare},
        {Relic::LongHand, "Long Hand", "Win with 4+ cards for +1 coin and +8 points.", 6, Rarity::Rare},
        {Relic::SecondChance, "Second Chance", "Discard your first bust card. Once per round.", 10, Rarity::Legendary},
        {Relic::Streak, "Streak", "+0.1x per earlier consecutive win (max 1.4x). Every third win earns +1 coin.", 8, Rarity::Epic},
        {Relic::RoyalPurse, "Royal Purse", "Win with a natural blackjack for +1 coin.", 2, Rarity::Common},
        {Relic::SoftTouch, "Soft Touch", "Win with a soft hand for +1 coin.", 2, Rarity::Common},
        {Relic::NarrowEscape, "Narrow Escape", "Win when the dealer busts for +1 coin and +3 points.", 6, Rarity::Rare},
        {Relic::RedRibbon, "Red Ribbon", "Win with two red opening cards for +1 coin.", 4, Rarity::Uncommon},
        {Relic::BlackVelvet, "Black Velvet", "Win with two black opening cards for +1 coin.", 4, Rarity::Uncommon},
        {Relic::EvenOdds, "Even Odds", "Win with an even hand total for +1 coin.", 4, Rarity::Uncommon},
        {Relic::CourtFavor, "Court Favor", "Win with at least one J, Q or K for +3 points.", 2, Rarity::Common},
        {Relic::HumbleCrown, "Humble Crown", "Win with 18 or less for +5 points.", 2, Rarity::Common}
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
            state_.score.bonus += 8;
        }
        if (hasRelic(Relic::Streak) && (state_.streak + 1) % 3 == 0) {
            ++state_.score.coins;
        }
        if (hasRelic(Relic::Precision) && player.total == 21) {
            ++state_.score.coins;
            state_.score.bonus += 5;
        }
        if (hasRelic(Relic::RoyalPurse) && player.blackjack) {
            ++state_.score.coins;
        }
        if (hasRelic(Relic::SoftTouch) && player.soft) {
            ++state_.score.coins;
        }
        if (hasRelic(Relic::NarrowEscape) && dealer.bust) {
            ++state_.score.coins;
            state_.score.bonus += 3;
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
        if (hasRelic(Relic::Streak)) {
            state_.score.multiplier += .1f * static_cast<float>(std::min(state_.streak, 4));
        }
        state_.score.total = static_cast<int>(std::lround(
            static_cast<float>(state_.score.base + state_.score.bonus) * state_.score.multiplier));
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
    state_.phase = state_.handsLeft == 0 && state_.roundScore < state_.goal ? Phase::RunLost : Phase::RoundOver;
    if (state_.phase == Phase::RunLost) {
        state_.message = "Out of hands before the goal. Start again and try another build.";
    }
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
    constexpr std::array<Rarity, 5> rarities{
        Rarity::Common, Rarity::Uncommon, Rarity::Rare, Rarity::Epic, Rarity::Legendary
    };
    state_.offers.clear();
    for (int slot = 0; slot < 2; ++slot) {
        std::array<std::vector<Relic>, 5> eligible;
        std::array<std::vector<Relic>, 5> fresh;
        for (const auto& item : relicCatalog()) {
            if (hasRelic(item.id) || std::find(state_.offers.begin(), state_.offers.end(), item.id) != state_.offers.end()) {
                continue;
            }
            const auto tier = static_cast<std::size_t>(item.rarity);
            eligible[tier].push_back(item.id);
            if (std::find(previousOffers_.begin(), previousOffers_.end(), item.id) == previousOffers_.end()) {
                fresh[tier].push_back(item.id);
            }
        }
        int totalWeight = 0;
        for (std::size_t tier = 0; tier < rarities.size(); ++tier) {
            if (!eligible[tier].empty()) {
                totalWeight += rarityWeight(rarities[tier]);
            }
        }
        if (totalWeight == 0) {
            break;
        }
        int roll = static_cast<int>(boundedRandom(shopRandom_, static_cast<std::uint32_t>(totalWeight)));
        for (std::size_t tier = 0; tier < rarities.size(); ++tier) {
            if (eligible[tier].empty()) {
                continue;
            }
            const int weight = rarityWeight(rarities[tier]);
            if (roll < weight) {
                // Prefer a fresh item inside the chosen rarity. An earlier offer may
                // repeat when necessary, so anti-repeat never removes an available tier.
                const auto& pool = fresh[tier].empty() ? eligible[tier] : fresh[tier];
                const auto index = boundedRandom(shopRandom_, static_cast<std::uint32_t>(pool.size()));
                state_.offers.push_back(pool[index]);
                break;
            }
            roll -= weight;
        }
    }
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
