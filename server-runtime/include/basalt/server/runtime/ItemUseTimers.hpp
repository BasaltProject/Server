#pragma once

#include <basalt/server/runtime/player/PlayerCrossbow.hpp>
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

/// Eating, drawing a bow and the cooldown clock: what a player is doing with the item in hand, and
/// for how much longer. Holds a back-reference to the session because every method needs several of
/// its collaborators (world, plugin hooks, outbound packets).
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

    /// Raises a shield from that slot, main hand or off hand. parity: `ShieldItem.use`.
    bool startBlocking(player::PlayerSession& session, const wire::common::Identifier& item, std::int32_t slot);

    void stopBlocking();

    /// One tick behind the shield. Moving it out of the hand lowers it.
    void tickBlocking(player::PlayerSession& session);

    /// parity: `EnderpearlItem.use`.
    bool throwEnderPearl(player::PlayerSession& session, const wire::common::Identifier& item, std::int32_t slot);

    /// parity: `SpyglassItem.use`: raised at once, a minute at most, the sound on each end.
    bool startSpyglass(player::PlayerSession& session, const wire::common::Identifier& item, std::int32_t slot);
    void stopSpyglass();
    void tickSpyglass(player::PlayerSession& session);

    /// parity: `ServerPlayer.updateUsingItem`, `USING_ITEM` every turn something is held in use.
    void tickUsingTrigger(player::PlayerSession& session);

    /// Whether this item is still greyed out. parity: `ItemCooldowns.isOnCooldown`.
    [[nodiscard]] bool onCooldown(const wire::common::Identifier& item);

    /// Starts the wait an item's own `use_cooldown` component asks for, and tells the client so the
    /// slot greys out.
    void startCooldown(const wire::common::Identifier& item);

    /// The "this one is using an item" flag, sent to everybody including its owner.
    void setUsingItem(bool usingItem, std::optional<wire::common::Identifier> item = std::nullopt,
                      bool offhand = false);

    /// parity: `BowItem.getPowerForTime`.
    [[nodiscard]] static float bowPower(std::int32_t ticks);

private:
    ClientSession* owner_;
    Hooks hooks_;
    Options options_;
    /// parity: `ItemStack.causeUseVibration`, which raises `ITEM_INTERACT_START` when a use begins and
    /// `ITEM_INTERACT_FINISH` when it ends, at the user's own position. The item is remembered because
    /// the end of a use has to know what was being used, and a spear is the one item the jar builds
    /// with `interact_vibrations` off.
    void raiseUseVibration(std::string_view event, const wire::common::Identifier& item);

    /// A game event at the player's own position, which is where every one of these is raised.
    void emitAt(std::string_view event);

    void playAroundExceptSelf(const player::PlayerSession& session, std::string_view sound);

    /// Which slot is being eaten from, and how many ticks of the mouthful are left.
    bool usingItem_ = false;
    /// What the using-item bit is set for, so the end of the use can name it.
    std::optional<wire::common::Identifier> usedItem_;
    std::int32_t eatingSlot_ = -1;
    /// The spyglass held up, and the turns left of its minute.
    std::int32_t spyglassSlot_ = -1;
    std::int32_t spyglassTicks_ = 0;
    std::int32_t eatingTicks_ = 0;
    /// What cannot be used again yet, by item path and the tick it frees up on.
    /// The bow being drawn, and for how long.
    std::int32_t drawingSlot_ = -1;
    std::int32_t drawingTicks_ = 0;
    /// The sounds a crossbow charge in progress has played.
    player::crossbow::Charging crossbowCharging_;
};

} // namespace basalt::server::runtime
