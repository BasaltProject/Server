#pragma once

#include <basalt/server/runtime/entity/ai/BrainKit.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <utility>
#include <vector>

namespace basalt::server::runtime::entity::ai {

/// parity: `LongJumpToRandomPos`.
namespace longJump {

struct Spec {
    std::array<std::int32_t, 2> cooldown{600, 1200};
    std::int32_t height = 5;
    std::int32_t width = 5;
    float velocityMultiplier = 3.5714288F;
    /// parity: the box the arc is checked with, the goat's shrunk to seven tenths while it jumps.
    float jumpingScale = 1.0F;
    /// parity: the landing spot's own test, `defaultAcceptableLandingSpot` unless the mob has one.
    std::function<bool(const brainKit::Block& target)> acceptable;
    /// parity: `LongJumpToPreferredBlock`: the chance to want a spot over this, and the test.
    float preferredChance = 0.0F;
    std::function<bool(const brainKit::Block& below)> preferred;
};

struct State {
    std::optional<std::int32_t> cooldown;
    bool midJump = false;
    bool jumping = false;
    std::int32_t run = 0;
    std::vector<std::pair<brainKit::Block, std::int32_t>> candidates;
    std::vector<std::pair<brainKit::Block, std::int32_t>> notPreferred;
    bool wantsPreferred = false;
    std::optional<brainKit::Vec> from;
    std::optional<brainKit::Vec> chosen;
    std::int32_t tries = 0;
    std::int64_t prepareStart = 0;
};

enum class Event : std::uint8_t { None, Jumped, Landed };

/// parity: `LongJumpUtil.calculateJumpVectorForAngle`.
[[nodiscard]] std::optional<brainKit::Vec> vectorForAngle(MobContext& context, const brainKit::Vec& target,
                                                          float maxVelocity, std::int32_t angle, double width,
                                                          double height, bool checkCollision);

/// parity: `defaultAcceptableLandingSpot`: something whole below and nothing in the way.
[[nodiscard]] bool defaultAcceptable(const MobContext& mob, const brainKit::Block& target);

[[nodiscard]] Event tick(MobContext& mob, State& state, const Spec& spec, brainKit::Look& look);

void countDown(State& state);

} // namespace longJump

} // namespace basalt::server::runtime::entity::ai
