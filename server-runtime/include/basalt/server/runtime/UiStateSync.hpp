#pragma once

#include <basalt/server/registry/ItemFood.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <vector>

namespace basalt::wire::protocol::play {
struct CommandsPacket;
}

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

class UiStateSync {
public:
    struct Hooks {
        std::function<bool(const player::PlayerSession& player, const wire::common::Identifier& effect,
                           std::int32_t amplifier, std::int32_t durationTicks)>
            effectAllowed;
        std::function<void(const player::PlayerSession& player, std::int32_t level, std::int32_t previous)>
            levelChanged;
    };

    explicit UiStateSync(ClientSession& owner, Hooks hooks = {});

    void syncHealth();

    void addEffect(const wire::common::Identifier& effect, std::int32_t durationTicks, std::int32_t amplifier);

    [[nodiscard]] bool effectAllowed(const wire::common::Identifier& effect, std::int32_t durationTicks,
                                     std::int32_t amplifier);

    void sendEffect(const world::ActiveEffect& effect);

    void sendEffectRemoved(const wire::common::Identifier& effect);

    void syncHeldAttributes();

    void forgetSentAttributes() { sentAttributes_.clear(); }

    /// Redraws whatever is due. Failures are swallowed: decoration must never drop a connection.
    void tickUi();

    void sendTime(const world::LoadedWorld& world);

    void sendWeather(const world::WorldWeather& weather);

    void syncExperience();

    void sendAbilities();

    void sendCommandTree();

    void sendStats();

    void applyConsumeEffects(player::PlayerSession& session,
                             const std::vector<server::registry::ConsumeEffect>& effects,
                             const std::function<float()>& nextFloat);

private:
    void filterCommandTree(wire::protocol::play::CommandsPacket& tree, const player::PlayerSession& session);

    /// parity: `PlayerList.sendPlayerPermissionLevel`.
    void sendPermissionLevel();

    ClientSession* owner_;
    Hooks hooks_;
    std::map<std::int32_t, wire::protocol::play::UpdateAttributesPacket::AttributeSnapshot> sentAttributes_;
};

} // namespace basalt::server::runtime
