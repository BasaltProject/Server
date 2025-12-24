#pragma once

#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/item/ItemComponentCodecs.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/MobEffects.hpp>
#include <basalt/server/runtime/world/WorldClock.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>
#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/protocol/play/SpawnInfo.hpp>
#include <basalt/wire/text/Component.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/// Declared, not included: a world is what `switchWorld` moves someone into, and the connection
/// only ever holds a reference to one.
namespace basalt::server::runtime::world {
class LoadedWorld;
} // namespace basalt::server::runtime::world

namespace basalt::server::runtime::player {

/// What a player's session needs of the connection they arrived on.
///
/// Five facts and one call, handed over rather than reached for: the session is what outlives a
/// packet exchange, and a connection is only how it reaches the client. Keeping the seam this
/// narrow is what lets the session, and everything built on it, be written and tested without the
/// networking around it.
struct PlayerConnection {
    /// What the client said in its handshake, and where it connected from: the same facts a ping
    /// carries.
    std::int32_t protocolVersion = 0;
    std::string connectedHost;
    std::optional<std::string> address;

    /// Enchantment name to registry index, or unset until the enchantment registry has gone out.
    item::itemComponentCodecs::EnchantmentId enchantmentIds;

    /// Sends a system message to this player. Unset, nothing is sent, which is what a session with
    /// no connection behind it does.
    std::function<void(const wire::text::Component&)> sendSystemMessage;

    /// Writes one play packet to this player. Unset, nothing is sent.
    ///
    /// The packet writes itself into the buffer the connection hands over, so nothing here needs to
    /// know what any of them look like.
    std::function<void(std::int32_t packetId, const std::function<void(wire::io::ByteBuffer&)>& write)> sendPlayPacket;

    /// What a block that changed asks everyone in this player's world to hear: a door, a note, a
    /// bell. One connection sends these to its whole world rather than only to itself.
    std::function<void(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
                       const std::vector<world::BlockEffect>& effects)>
        broadcastEffects;

    /// Sends one chunk again, for a change too big to patch block by block.
    std::function<void(std::int32_t chunkX, std::int32_t chunkZ)> resendChunk;

    /// Sends new biomes for these chunk columns without resending the columns themselves.
    std::function<void(const std::vector<std::pair<std::int32_t, std::int32_t>>& columns)> sendBiomes;

    /// Pushes the world's weather, which is three game events rather than one packet.
    std::function<void(const world::WorldWeather& weather)> sendWeather;

    /// Closes this connection with a reason the client shows. Unset, nothing happens.
    std::function<void(const wire::text::Component& reason)> disconnect;

    /// Puts this player in a game mode, which is a packet and a change on the session at once.
    std::function<void(wire::protocol::play::Gamemode mode)> setGamemode;

    /// Kills this player with the death message id vanilla would use.
    /// Re-sends every inventory slot, which is how a change made on the server side is believed:
    /// the client draws from its own copy and never asks.
    std::function<void()> sendInventoryAll;

    /// Gives health back, up to whatever their maximum is.
    std::function<void(float amount)> heal;

    /// Damage that goes through the ordinary path: armour, absorption, the death message and the
    /// hurt animation. The message id is what a death sentence is looked up by, the type is what
    /// the tags are tested against.
    ///
    /// The source travels with it because only the caller knows it: below there is an id and no way
    /// to name whoever swung.
    std::function<void(float amount, std::string_view deathMessageId, const wire::common::Identifier& damageType,
                       const std::optional<entity::DamageSource>& source)>
        hurt;

    /// Tells the client an effect started, or that it is over. The client draws its own icon and
    /// screen tint from these and from nothing else.
    /// Starts an effect on this player, which is what a splash potion and a command both do.
    std::function<void(const wire::common::Identifier& effect, std::int32_t durationTicks, std::int32_t amplifier)>
        addEffect;

    std::function<void(const world::ActiveEffect& effect)> sendEffect;
    std::function<void(const wire::common::Identifier& effect)> sendEffectRemoved;

    /// Moves the player to another world, at the coordinates given. Answers false when the move did
    /// not happen, which is how a command reports that nobody went anywhere.
    std::function<bool(world::LoadedWorld& world, double x, double y, double z)> switchWorld;

    /// Moves the player and tells their client, which is the only way a position set on the server
    /// is believed: a client that was not told rubber-bands straight back.
    std::function<void(double x, double y, double z, float yaw, float pitch)> teleportTo;

    /// Gets this player out of the bed they are in, which is a packet and a pose at once.
    std::function<void()> wakeUp;

    /// Pushes the world's clock, which the client runs on its own between updates.
    std::function<void(const world::LoadedWorld& world)> sendTime;

    /// Pushes the air supply and the burning flag, which the HUD draws its bubbles and flames from.
    std::function<void()> syncSubmersion;

    /// Re-sends the command tree, which is filtered by permission and so goes stale the moment a
    /// player's group changes.
    std::function<void()> sendCommandTree;

    /// Pushes this player's level and bar after something moved them, since the client draws its
    /// own copy and never asks.
    std::function<void()> syncExperience;

    std::function<void(std::string_view deathMessageId)> killPlayer;

    /// This player as the database stores them, or nothing when there is nothing worth writing yet.
    ///
    /// The connection owns it because what a saved player is made of (position, inventory, effects)
    /// lives across the whole session, not in any one of the pieces that change it.
    std::function<std::optional<storage::StoredPlayer>()> storedSnapshot;
};

} // namespace basalt::server::runtime::player
