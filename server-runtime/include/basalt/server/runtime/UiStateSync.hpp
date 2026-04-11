#pragma once

#include <basalt/server/registry/ItemFood.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <vector>

namespace basalt::server::runtime::player {
class PlayerSession;
}

namespace basalt::server::runtime::world {
class LoadedWorld;
struct ActiveEffect;
class WorldWeather;
} // namespace basalt::server::runtime::world

namespace basalt::server::runtime {

class ClientSession;

/// The HUD-facing sync surface: health, effects, absorption, experience, abilities, the command
/// tree, statistics and the sidebar, scoreboard and boss bar redraw.
///
/// A back-reference to the owning session rather than narrow callbacks, the same pattern as the
/// other satellites: this state was already tightly coupled to the session, and every method here
/// still needs a handful of its collaborators.
class UiStateSync {
public:
    /// What a plugin gets a say in. Unset means nobody objects.
    struct Hooks {
        std::function<bool(const player::PlayerSession& player, const wire::common::Identifier& effect,
                           std::int32_t amplifier, std::int32_t durationTicks)>
            effectAllowed;
        std::function<void(const player::PlayerSession& player, std::int32_t level, std::int32_t previous)>
            levelChanged;
    };

    explicit UiStateSync(ClientSession& owner, Hooks hooks = {});

    /// How this player looks to everyone else: crouched, sprinting, burning, and the pose that goes
    /// with it. Sent only when it changes.
    void syncPosture();

    /// The air left and whether this player burns, to them and to everyone who can see them.
    void syncSubmersion();

    void syncHealth();

    /// parity: `LivingEntity.addEffect`, for the effects a mob puts on somebody rather than a drink.
    void addEffect(const wire::common::Identifier& effect, std::int32_t durationTicks, std::int32_t amplifier);

    /// Sends one effect, or its removal when it has run out.
    void sendEffect(const world::ActiveEffect& effect);

    void sendEffectRemoved(const wire::common::Identifier& effect);

    /// parity: `LivingEntity.detectEquipmentUpdates` and the dirty attributes `ServerEntity.sendDirtyEntityData`
    /// sends: every attribute whose base or modifiers changed since last time, each whole.
    void syncHeldAttributes();

    /// For a respawn, whose new client-side player knows no modifiers.
    void forgetSentAttributes() { sentAttributes_.clear(); }

    /// Redraws whatever is due. Failures are swallowed: decoration must never drop a connection.
    void tickUi();

    /// parity: absorption rides on the player's own entity data, not on `set_health`.
    void syncAbsorption();

    /// This world's clocks, as the join sequence and every `/time` send them.
    void sendTime(const world::LoadedWorld& world);

    void sendWeather(const world::WorldWeather& weather);

    void syncExperience();

    void sendAbilities();

    /// Sends the command tree this player may use, which is what makes their client parse commands
    /// at all: with no tree, everything after a slash is unknown client-side.
    void sendCommandTree();

    /// The statistics screen's contents.
    void sendStats();

    /// parity: `Consumable.onConsume`, which is what makes a golden apple worth eating.
    void applyConsumeEffects(player::PlayerSession& session,
                             const std::vector<server::registry::ConsumeEffect>& effects,
                             const std::function<float()>& nextFloat);

private:
    /// parity: `PlayerList.sendPlayerPermissionLevel`, which goes right before the command tree.
    void sendPermissionLevel();

    ClientSession* owner_;
    Hooks hooks_;
    /// What the client was last told of each attribute, by its protocol id.
    std::map<std::int32_t, wire::protocol::play::UpdateAttributesPacket::AttributeSnapshot> sentAttributes_;
};

} // namespace basalt::server::runtime
