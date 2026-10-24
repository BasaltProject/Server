#pragma once

#include <basalt/server/runtime/Announcement.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>
#include <basalt/wire/protocol/play/SpawnInfo.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <utility>

namespace basalt::server::runtime::player {
class PlayerSession;
}

namespace basalt::server::runtime::world {
class AdvancementTriggers;
class LoadedWorld;
} // namespace basalt::server::runtime::world

namespace basalt::server::runtime {

class ClientSession;

class PlayerLifecycle {
public:
    struct Hooks {
        std::function<void(const player::PlayerSession& player, const world::LoadedWorld& world, double& x, double& y,
                           double& z)>
            respawned;
        std::function<bool(const player::PlayerSession& player, wire::protocol::play::Gamemode mode,
                           wire::protocol::play::Gamemode previous)>
            gamemodeAllowed;
        std::function<void(const player::PlayerSession& player, const world::LoadedWorld& to,
                           const std::optional<std::string>& from)>
            worldChanged;
        std::function<bool(const player::PlayerSession* player, double toX, double toY, double toZ,
                           std::string_view cause)>
            teleportAllowed;
        std::function<Announcement(const player::PlayerSession& player, const std::optional<std::string>& world)> quit;
    };

    explicit PlayerLifecycle(ClientSession& owner, Hooks hooks = {});

    void tickPlayer();

    void rideTick(player::PlayerSession& session);

    void respawn();

    /// What `CraftPlayer.refreshPlayer` does after a skin changes: the tab list entry again and a respawn in place.
    void refreshProfile();

    void setGamemode(wire::protocol::play::Gamemode mode);

    bool switchWorld(world::LoadedWorld& target, double x, double y, double z);

    void teleportTo(double x, double y, double z, float yaw, float pitch, std::string_view cause);

    void leavePlayIfPresent();

    void savePlayerState();

    [[nodiscard]] std::optional<storage::StoredPlayer> storedSnapshot() const;

private:
    void checkAdvancements(player::PlayerSession& session);

    void checkEquipment(player::PlayerSession& session);

    void tickTriggers(player::PlayerSession& session);

    void fireEffectsChanged(player::PlayerSession& session, world::AdvancementTriggers& triggers);

    /// parity: `ServerPlayer.triggerDimensionChangeTriggers`, after the player has arrived.
    void dimensionTriggers(player::PlayerSession& session, const world::LoadedWorld& previous,
                           const world::LoadedWorld& target, const std::array<double, 3>& left);

    ClientSession* owner_;
    Hooks hooks_;
    std::optional<std::array<std::optional<storage::StoredItemStack>, 4>> worn_;
    std::map<std::string, std::pair<std::int32_t, std::int32_t>, std::less<>> seenEffects_;
    std::optional<std::array<double, 3>> levitationStart_;
    std::int32_t levitationTicks_ = 0;
    std::optional<std::array<double, 3>> enteredNether_;
    std::optional<std::array<double, 3>> enteredLavaOnVehicle_;
};

} // namespace basalt::server::runtime
