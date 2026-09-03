#pragma once

#include <basalt/server/storage/StoredPlayer.hpp>
#include <basalt/wire/protocol/play/SpawnInfo.hpp>

#include <functional>
#include <optional>
#include <string>

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
        /// Answers the food level to settle on, or nothing to leave it where it was.
        std::function<std::optional<std::int32_t>(const player::PlayerSession& player, std::int32_t food,
                                                  std::int32_t previous)>
            foodChanged;
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

    ClientSession* owner_;
    Hooks hooks_;
};

} // namespace basalt::server::runtime
