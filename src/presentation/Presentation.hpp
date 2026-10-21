#pragma once

#include "game/Game.hpp"

#include <cstddef>
#include <vector>

namespace presentation {

enum class Action { Deal, Hit, Stand, Other };

// Owns only a paced view of snapshots; game commands and randomness stay in Game.
class Presentation {
public:
    void reset(const bj::Snapshot& snapshot);
    void begin(const bj::Snapshot& before, const bj::Snapshot& after, Action action);
    void update(float dt, float speed = 0.0f);
    void accelerate();

    bool busy() const;
    const bj::Snapshot& visible() const;
    float playerArrival() const;
    float dealerArrival() const;
    float dealerFlip() const;
    bool dealerFlipping() const;
    bool dealerRevealed() const;
    float dealerReach() const;
    bool dealingToPlayer() const;
    float celebration() const;
    float legendPulse() const;

    // -1: none; 0: base; 1..N: applied effects; N+1: final total.
    // The final step and effect count remain available after busy() becomes false.
    int rewardStep() const;
    std::size_t rewardVisibleEffects() const;

private:
    enum class Kind { PlayerCard, DealerCard, Flip, Rescue, RewardBase, RewardEffect, RewardTotal };
    struct Stage {
        Kind kind;
        std::size_t index = 0;
    };

    bj::Snapshot visible_;
    bj::Snapshot final_;
    std::vector<Stage> stages_;
    std::size_t stage_ = 0;
    float progress_ = 0.0f;
    bool dealerRevealed_ = true;
    float celebration_ = 0.0f;
    float legendPulse_ = 0.0f;
    int rewardStep_ = -1;
    std::size_t rewardVisibleEffects_ = 0;

    bool is(Kind kind) const;
    void enterStage();
    void finishStage();
    void decayPulses(float dt);
    float duration(float speed) const;
};

} // namespace presentation
