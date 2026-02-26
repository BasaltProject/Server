#include <basalt/server/runtime/player/PlayerBlockUpdates.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/wire/protocol/play/WorldPackets.hpp>

namespace basalt::server::runtime::player {

void sendBlockChangeTo(const PlayerSession& player, const std::int32_t x, const std::int32_t y, const std::int32_t z,
                       const std::int32_t stateId) {
    namespace play = wire::protocol::play;

    if (player.connection().sendBlockChange) {
        player.connection().sendBlockChange(x, y, z, stateId);
        return;
    }
    if (player.connection().sendPlayPacket) {
        const play::ClientboundBlockUpdatePacket packet{.pos = play::BlockPos{x, y, z}, .blockStateId = stateId};
        player.connection().sendPlayPacket(play::ClientboundBlockUpdatePacket::kId,
                                           [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
    }
}

} // namespace basalt::server::runtime::player
