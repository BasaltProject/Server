#pragma once

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
class LoadedWorld;
}

namespace basalt::server::runtime {

class ClientSession;

/// The player tick, advancements, respawn, game mode, world changes, teleporting, leaving and the
/// player row's persistence. The session's own `setGamemode`, `switchWorld`, `teleportTo` and
/// `storedSnapshot` stay thin delegates onto this class, since their callers (commands, portals,
/// the plugin bridge) are too wide to move wholesale.
class PlayerLifecycle {
public:
    /// What a plugin gets a say in, and what it is told. Unset means nobody objects.
    struct Hooks {
        std::function<void(const player::PlayerSession& player, const world::LoadedWorld& world, double x, double y,
                           double z)>
            respawned;
        /// Answers false to keep them in the mode they are in, with a line for the player.
        std::function<bool(const player::PlayerSession& player, wire::protocol::play::Gamemode mode,
                           wire::protocol::play::Gamemode previous)>
            gamemodeAllowed;
        std::function<void(const player::PlayerSession& player, const world::LoadedWorld& to,
                           const std::optional<std::string>& from)>
            worldChanged;
        std::function<bool(const player::PlayerSession* player, double toX, double toY, double toZ,
                           std::string_view cause)>
            teleportAllowed;
        std::function<void(const player::PlayerSession& player, const std::optional<std::string>& world)> quit;
    };

    explicit PlayerLifecycle(ClientSession& owner, Hooks hooks = {});

    /// One player tick, driven from this connection's own outbound loop rather than the tick loop.
    ///
    /// That loop already runs at twenty a second on this connection's thread, which is where
    /// anything that writes to this socket has to happen: hunger and regeneration both end in a
    /// `set_health` packet, so ticking them from the shared tick thread would park every other
    /// player behind a client that has stopped reading.
    void tickPlayer();

    /// parity: `ServerPlayer.rideTick`: a rider is carried where its vehicle is, and counts it.
    void rideTick(player::PlayerSession& session);

    /// parity: `PlayerList.respawn`, in the order it sends things: respawn, then position, then the
    /// spawn point, then experience.
    void respawn();

    /// parity: `ServerPlayer.setGameMode`, which tells the client through a game event rather than
    /// a dedicated packet, then re-sends abilities because the new mode decides them.
    void setGamemode(wire::protocol::play::Gamemode mode);

    /// Moves this player into another world, or does nothing if they are already in it.
    bool switchWorld(world::LoadedWorld& target, double x, double y, double z);

    void teleportTo(double x, double y, double z, float yaw, float pitch, std::string_view cause);

    void leavePlayIfPresent();

    /// Writes this player's row. Called on logout and from the periodic flush.
    void savePlayerState();

    /// This player's row as it stands right now, for the batched periodic flush.
    [[nodiscard]] std::optional<storage::StoredPlayer> storedSnapshot() const;

private:
    /// Fires the inventory trigger once a tick, when something moved.
    void checkAdvancements(player::PlayerSession& session);

    /// parity: `LivingEntity.handleEquipmentChanges`, which raises `EQUIP` or `UNEQUIP` for a slot
    /// whose stack moved, and `Player.doesEmitEquipEvent` narrows that to the four armour slots. It
    /// is silent on the first tick, which is what the empty snapshot below stands for.
    void checkEquipment(player::PlayerSession& session);

    /// parity: `ServerPlayer.tick`'s `TICK` and `LEVITATION`, and `EFFECTS_CHANGED` from
    /// `onEffectAdded`, `onEffectUpdated` and `onEffectsRemoved`, seen as the effects differ from
    /// last turn's since they are written in many places.
    void tickTriggers(player::PlayerSession& session);

    /// parity: `ServerPlayer.triggerDimensionChangeTriggers`, after the player has arrived.
    void dimensionTriggers(player::PlayerSession& session, const world::LoadedWorld& previous,
                           const world::LoadedWorld& target, const std::array<double, 3>& left);

    ClientSession* owner_;
    Hooks hooks_;
    /// What the four armour slots held last turn, to see what moved. Empty until the first turn.
    std::optional<std::array<std::optional<storage::StoredItemStack>, 4>> worn_;
    /// The effects as they stood last turn, by id: amplifier and ticks left.
    std::map<std::string, std::pair<std::int32_t, std::int32_t>, std::less<>> seenEffects_;
    /// parity: `levitationStartPos` and how many turns ago levitation began.
    std::optional<std::array<double, 3>> levitationStart_;
    std::int32_t levitationTicks_ = 0;
    /// parity: `enteredNetherPosition`, where the overworld was left for the nether.
    std::optional<std::array<double, 3>> enteredNether_;
};

} // namespace basalt::server::runtime
