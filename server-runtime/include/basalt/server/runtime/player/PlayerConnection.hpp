#pragma once

#include <basalt/server/runtime/item/ItemComponentCodecs.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldClock.hpp>
#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/text/Component.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

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
};

} // namespace basalt::server::runtime::player
