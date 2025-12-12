#pragma once

#include <basalt/server/runtime/item/ItemComponentCodecs.hpp>
#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/text/Component.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

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
};

} // namespace basalt::server::runtime::player
