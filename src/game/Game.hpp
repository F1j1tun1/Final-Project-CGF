#pragma once

#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace bj {

enum class Suit { Clubs, Diamonds, Hearts, Spades };

struct Card {
    int rank;
    Suit suit;
};

struct HandValue {
    int total = 0;
    bool soft = false;
    bool blackjack = false;
    bool bust = false;
};

HandValue evaluateHand(const std::vector<Card>& hand);

enum class Outcome { None, Win, Loss, Push };
enum class Phase { Ready, PlayerTurn, RoundOver, Shop, RunLost };
enum class Relic {
    Precision, LongHand, SecondChance, Streak, RoyalPurse, SoftTouch,
    NarrowEscape, RedRibbon, BlackVelvet, EvenOdds, CourtFavor, HumbleCrown
};

enum class Rarity { Common, Uncommon, Rare, Epic, Legendary };
const char* rarityName(Rarity rarity);
int rarityWeight(Rarity rarity);

struct RelicInfo {
    Relic id;
    const char* name;
    const char* description;
    int cost;
    Rarity rarity;
};

const std::vector<RelicInfo>& relicCatalog();

struct ScoreBreakdown {
    int base = 0;
    int bonus = 0;
    float multiplier = 1.0f;
    int total = 0;
    int coins = 0;
};

struct Snapshot {
    std::vector<Card> player;
    std::vector<Card> dealer;
    Phase phase = Phase::Ready;
    Outcome outcome = Outcome::None;
    int round = 1;
    int handsLeft = 5;
    int roundScore = 0;
    int goal = 30;
    int wins = 0;
    int totalScore = 0;
    int coins = 0;
    int streak = 0;
    int dealerStand = 17;
    std::uint32_t seed = 42;
    bool usedSecondChance = false;
    std::vector<Relic> owned;
    std::vector<Relic> offers;
    ScoreBreakdown score;
    std::string message;
};

class Game {
public:
    explicit Game(std::uint32_t seed = 42);

    const Snapshot& state() const;
    void restart(std::uint32_t seed);
    bool deal();
    bool hit();
    bool stand();
    bool advance();
    bool buy(Relic relic);
    bool sell(Relic relic);
    bool leaveShop();
    bool hasRelic(Relic relic) const;
    bool boughtThisShop(Relic relic) const;

private:
    Snapshot state_;
    std::mt19937 cardRandom_;
    std::mt19937 shopRandom_;
    std::vector<Card> deck_;
    std::vector<Relic> previousOffers_;
    std::vector<Relic> boughtThisShop_;

    void shuffleDeck();
    Card draw();
    void finishRound();
    void openShop();
};

} // namespace bj
