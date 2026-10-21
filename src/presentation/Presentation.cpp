#include "Presentation.hpp"

#include <algorithm>
#include <cmath>

namespace presentation {
namespace {
constexpr float pi = 3.14159265358979323846f;

bool owns(const bj::Snapshot& snapshot, bj::Relic relic) {
    return std::find(snapshot.owned.begin(), snapshot.owned.end(), relic) != snapshot.owned.end();
}

bool legendary(bj::Relic relic) {
    const auto& catalog = bj::relicCatalog();
    const auto item = std::find_if(catalog.begin(), catalog.end(), [relic](const bj::RelicInfo& info) {
        return info.id == relic;
    });
    return item != catalog.end() && item->rarity == bj::Rarity::Legendary;
}
} // namespace

void Presentation::reset(const bj::Snapshot& snapshot) {
    visible_ = final_ = snapshot;
    stages_.clear();
    stage_ = 0;
    progress_ = 0.0f;
    dealerRevealed_ = snapshot.phase != bj::Phase::PlayerTurn;
    celebration_ = legendPulse_ = 0.0f;
    rewardStep_ = snapshot.outcome == bj::Outcome::Win
        ? static_cast<int>(snapshot.score.appliedEffects.size()) + 1 : -1;
    rewardVisibleEffects_ = snapshot.outcome == bj::Outcome::Win
        ? snapshot.score.appliedEffects.size() : 0;
}

void Presentation::begin(const bj::Snapshot& before, const bj::Snapshot& after, Action action) {
    reset(before);
    final_ = after;
    rewardStep_ = -1;
    rewardVisibleEffects_ = 0;
    const bool rescued = !before.usedSecondChance && after.usedSecondChance;
    const bool acquiredLegend = std::any_of(after.owned.begin(), after.owned.end(), [&](bj::Relic relic) {
        return legendary(relic) && !owns(before, relic);
    });
    if (action == Action::Other) {
        reset(after);
        if (rescued || acquiredLegend) legendPulse_ = 1.0f;
        return;
    }

    visible_.phase = bj::Phase::PlayerTurn;
    visible_.outcome = bj::Outcome::None;
    visible_.score = bj::ScoreBreakdown{};
    visible_.handsLeft = after.handsLeft;
    visible_.message = action == Action::Deal ? "Dealing the cards..." : "The cards are settling...";
    dealerRevealed_ = before.phase != bj::Phase::PlayerTurn && action != Action::Deal;

    if (action == Action::Deal) {
        visible_.player.clear();
        visible_.dealer.clear();
        const auto count = std::max(after.player.size(), after.dealer.size());
        for (std::size_t i = 0; i < count; ++i) {
            if (i < after.player.size()) stages_.push_back({Kind::PlayerCard, i});
            if (i < after.dealer.size()) stages_.push_back({Kind::DealerCard, i});
        }
    } else {
        for (std::size_t i = before.player.size(); i < after.player.size(); ++i)
            stages_.push_back({Kind::PlayerCard, i});
    }

    if (rescued) stages_.push_back({Kind::Rescue, 0});
    if (after.outcome != bj::Outcome::None) {
        if (!dealerRevealed_ && after.dealer.size() >= 2) stages_.push_back({Kind::Flip, 0});
        if (action != Action::Deal) {
            for (std::size_t i = before.dealer.size(); i < after.dealer.size(); ++i)
                stages_.push_back({Kind::DealerCard, i});
        }
        if (after.outcome == bj::Outcome::Win) {
            stages_.push_back({Kind::RewardBase, 0});
            for (std::size_t i = 0; i < after.score.appliedEffects.size(); ++i)
                stages_.push_back({Kind::RewardEffect, i});
            stages_.push_back({Kind::RewardTotal, 0});
        }
    }
    if (stages_.empty()) {
        reset(after);
    } else {
        enterStage();
    }
    if (acquiredLegend) legendPulse_ = 1.0f;
}

void Presentation::enterStage() {
    progress_ = 0.0f;
    const auto& current = stages_[stage_];
    switch (current.kind) {
    case Kind::PlayerCard:
        visible_.player.push_back(final_.player[current.index]);
        break;
    case Kind::DealerCard:
        visible_.dealer.push_back(final_.dealer[current.index]);
        break;
    case Kind::Flip:
        break;
    case Kind::Rescue:
        legendPulse_ = 1.0f;
        break;
    case Kind::RewardBase:
        visible_ = final_;
        dealerRevealed_ = true;
        rewardStep_ = 0;
        celebration_ = 1.0f;
        break;
    case Kind::RewardEffect:
        rewardStep_ = static_cast<int>(current.index) + 1;
        rewardVisibleEffects_ = current.index + 1;
        if (legendary(final_.score.appliedEffects[current.index].relic))
            legendPulse_ = 1.0f;
        break;
    case Kind::RewardTotal:
        rewardStep_ = static_cast<int>(final_.score.appliedEffects.size()) + 1;
        rewardVisibleEffects_ = final_.score.appliedEffects.size();
        celebration_ = 1.0f;
        break;
    }
}

void Presentation::finishStage() {
    if (!busy()) return;
    if (is(Kind::Flip)) dealerRevealed_ = true;
    ++stage_;
    if (busy()) {
        enterStage();
    } else {
        visible_ = final_;
        dealerRevealed_ = final_.phase != bj::Phase::PlayerTurn;
        progress_ = 0.0f;
    }
}

float Presentation::duration(float speed) const {
    float natural = 0.55f;
    if (is(Kind::Flip) || is(Kind::RewardBase)) natural = 0.4f;
    else if (is(Kind::Rescue) || is(Kind::RewardEffect)) natural = 0.5f;
    else if (is(Kind::RewardTotal)) natural = 0.6f;
    return natural + (0.07f - natural) * speed;
}

void Presentation::decayPulses(float dt) {
    celebration_ = std::max(0.0f, celebration_ - dt / 1.1f);
    legendPulse_ = std::max(0.0f, legendPulse_ - dt / 0.9f);
}

void Presentation::update(float dt, float speed) {
    if (!std::isfinite(dt) || dt <= 0.0f) return;
    speed = std::isfinite(speed) ? std::clamp(speed, 0.0f, 1.0f) : 0.0f;
    float remaining = dt;
    while (busy() && remaining > 0.0f) {
        const float stageDuration = duration(speed);
        const float needed = (1.0f - progress_) * stageDuration;
        const float consumed = std::min(remaining, needed);
        decayPulses(consumed);
        remaining -= consumed;
        progress_ = std::min(1.0f, progress_ + consumed / stageDuration);
        if (progress_ >= 1.0f - 0.000001f) finishStage();
        else break;
    }
    if (!busy() && remaining > 0.0f) decayPulses(remaining);
}

void Presentation::accelerate() { finishStage(); }
bool Presentation::busy() const { return stage_ < stages_.size(); }
const bj::Snapshot& Presentation::visible() const { return visible_; }
bool Presentation::is(Kind kind) const { return busy() && stages_[stage_].kind == kind; }
float Presentation::playerArrival() const { return is(Kind::PlayerCard) ? progress_ : 1.0f; }
float Presentation::dealerArrival() const { return is(Kind::DealerCard) ? progress_ : 1.0f; }
float Presentation::dealerFlip() const { return dealerFlipping() ? progress_ : 1.0f; }
bool Presentation::dealerFlipping() const { return is(Kind::Flip); }
bool Presentation::dealerRevealed() const { return dealerRevealed_; }
float Presentation::dealerReach() const {
    return is(Kind::PlayerCard) || is(Kind::DealerCard) ? std::sin(pi * progress_) : 0.0f;
}
bool Presentation::dealingToPlayer() const { return is(Kind::PlayerCard); }
float Presentation::celebration() const { return celebration_; }
float Presentation::legendPulse() const { return legendPulse_; }
int Presentation::rewardStep() const { return rewardStep_; }
std::size_t Presentation::rewardVisibleEffects() const { return rewardVisibleEffects_; }

} // namespace presentation
