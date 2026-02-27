#include <basalt/server/runtime/player/PlayerParticles.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>

namespace basalt::server::runtime::player {

void showParticleFor(const PlayerSession& player, const std::string_view particle,
                     const wire::protocol::play::LevelParticlesPacket& packet) {
    if (player.connection().showParticle) {
        player.connection().showParticle(particle, packet.x, packet.y, packet.z);
        return;
    }
    if (player.connection().sendPlayPacket) {
        player.connection().sendPlayPacket(wire::protocol::play::LevelParticlesPacket::kId,
                                           [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
    }
}

} // namespace basalt::server::runtime::player
