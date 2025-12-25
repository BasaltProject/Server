#pragma once

#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <map>
#include <optional>

namespace basalt::server::runtime::player {
class PlayerSession;
}

namespace basalt::server::runtime {

class ClientSession;

/// What a player is in the middle of doing with what they are holding: eating, drawing a bow, and
/// the cooldowns an item leaves behind.
class ItemUseTimers {
public:
    explicit ItemUseTimers(ClientSession& owner);

    void startEating(player::PlayerSession& session, const wire::common::Identifier& item, std::int32_t ticks);

    void stopEating();

    void tickEating(player::PlayerSession& session);

    void startDrawing(const wire::common::Identifier& item);

    void stopDrawing();

    void tickDrawing(player::PlayerSession& session);

    /// parity: `BowItem.releaseUsing`, whose arrow is faster the longer the bow was held.
    void releaseBow();

    void throwEnderPearl(player::PlayerSession& session);

    /// parity: `ItemCooldowns`, which is what stops an ender pearl being thrown twice a second.
    [[nodiscard]] bool onCooldown(const wire::common::Identifier& item) const;

    void startCooldown(const wire::common::Identifier& item);

    /// The shared flag that draws the using-item animation on every other client.
    void setUsingItem(bool using_);

    /// parity: `BowItem.getPowerForTime`, the curve a draw follows.
    [[nodiscard]] static float bowPower(std::int32_t ticks);

private:
    ClientSession* owner_;
    std::optional<wire::common::Identifier> eating_;
    std::int32_t eatTicksLeft_ = 0;
    std::optional<wire::common::Identifier> drawing_;
    std::int32_t drawTicks_ = 0;
    /// The game time each item is on cooldown until.
    std::map<wire::common::Identifier, std::int64_t> cooldowns_;
    bool usingItem_ = false;
};

} // namespace basalt::server::runtime
