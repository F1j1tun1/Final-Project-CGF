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
enum class Phase { Ready, PlayerTurn, RoundOver, Shop, RunWon, RunLost };
enum class Relic { Precision, LongHand, SecondChance, Streak };

struct RelicInfo {
    Relic id;
    const char* name;
    const char* description;
    int cost;
};

const std::vector<RelicInfo>& relicCatalog();

struct ScoreBreakdown {
    int base = 0;
    int bonus = 0;
    float multiplier = 1.0f;
    int total = 0;
};

struct Snapshot {
    std::vector<Card> player;
    std::vector<Card> dealer;
    Phase phase = Phase::Ready;
    Outcome outcome = Outcome::None;
    int stage = 1;
    int goal = 30;
    int stageScore = 0;
    int roundsLeft = 5;
    int coins = 0;
    int streak = 0;
    int dealerStand = 17;
    std::uint32_t seed = 42;
    bool usedSecondChance = false;
    std::vector<Relic> owned;
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
    bool leaveShop();
    bool hasRelic(Relic relic) const;

private:
    Snapshot state_;
    std::mt19937 random_;
    std::vector<Card> deck_;

    void shuffleDeck();
    Card draw();
    void finishRound();
};

} // namespace bj
