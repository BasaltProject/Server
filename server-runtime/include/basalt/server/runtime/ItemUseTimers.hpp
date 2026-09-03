#pragma once

#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <string>

namespace basalt::server::runtime::player {
class PlayerSession;
}

namespace basalt::server::runtime {

class ClientSession;

/// Eating, drawing a bow and the cooldown clock: what a player is doing with the item in their
/// hand, and for how much longer.
///
/// A back-reference to the owning session rather than narrow callbacks: this state was already
/// tightly coupled to the session, and every method here still needs a handful of its
/// collaborators (the world, the plugin hooks, the outbound packets).
class ItemUseTimers {
public:
    /// What a plugin gets a say in. Unset means nobody objects.
    struct Hooks {
        std::function<bool(const player::PlayerSession& player, const wire::common::Identifier& item)> consumeAllowed;
        std::function<bool(const player::PlayerSession& player, const wire::common::Identifier& projectile,
                           float power)>
            shootAllowed;
    };

    /// The rolls a shot makes, which are not part of any world stream.
    struct Options {
        std::function<std::int64_t()> nextSeed;
        std::function<float()> nextFloat;
    };

    explicit ItemUseTimers(ClientSession& owner, Hooks hooks = {}, Options options = {});

    /// Starts eating what is in hand, if it is food and there is room for it.
    bool startEating(player::PlayerSession& session, const wire::common::Identifier& item, std::int32_t slot);

    void stopEating();

    /// One tick of the mouthful, and the swallow at the end of it.
    void tickEating(player::PlayerSession& session);

    /// Starts drawing a bow, if there is anything to shoot with it.
    bool startDrawing(player::PlayerSession& session, const wire::common::Identifier& item, std::int32_t slot);

    void stopDrawing();

    /// One tick of the draw. Letting go of the bow, or of the slot it is in, gives it up.
    void tickDrawing(player::PlayerSession& session);

    /// parity: `BowItem.releaseUsing`: the power is the square of the draw, a shot under a tenth is
    /// no shot at all, and a full draw flies at three times that and counts as a critical.
    void releaseBow();

    /// parity: `EnderpearlItem.use`.
    bool throwEnderPearl(player::PlayerSession& session, const wire::common::Identifier& item, std::int32_t slot);

    /// Whether this item is still greyed out. parity: `ItemCooldowns.isOnCooldown`.
    [[nodiscard]] bool onCooldown(const wire::common::Identifier& item);

    /// Starts the wait an item's own `use_cooldown` component asks for, and tells the client so the
    /// slot greys out.
    void startCooldown(const wire::common::Identifier& item);

    /// The "this one is using an item" flag, sent to everybody including its owner.
    void setUsingItem(bool usingItem);

    /// parity: `BowItem.getPowerForTime`.
    [[nodiscard]] static float bowPower(std::int32_t ticks);

private:
    ClientSession* owner_;
    Hooks hooks_;
    Options options_;
    /// Which slot is being eaten from, and how many ticks of the mouthful are left.
    bool usingItem_ = false;
    std::int32_t eatingSlot_ = -1;
    std::int32_t eatingTicks_ = 0;
    /// What cannot be used again yet, by item path and the tick it frees up on.
    std::map<std::string, std::int64_t, std::less<>> itemCooldowns_;
    /// The bow being drawn, and for how long.
    std::int32_t drawingSlot_ = -1;
    std::int32_t drawingTicks_ = 0;
};

} // namespace basalt::server::runtime
