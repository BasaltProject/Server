#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/player/PlayerSounds.hpp>

namespace basalt::server::runtime::player {

void playSoundFor(const PlayerSession& player, const std::string_view sound,
                  const wire::protocol::play::SoundSource source, const double x, const double y, const double z,
                  const float volume, const float pitch, const std::int64_t seed) {
    namespace play = wire::protocol::play;

    if (player.connection().playSound) {
        player.connection().playSound(sound, x, y, z, volume, pitch);
        return;
    }
    if (player.connection().sendPlayPacket) {
        const play::SoundPacket packet{.sound = std::string{sound},
                                       .source = source,
                                       .x = x,
                                       .y = y,
                                       .z = z,
                                       .volume = volume,
                                       .pitch = pitch,
                                       .seed = seed};
        player.connection().sendPlayPacket(play::SoundPacket::kId,
                                           [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
    }
}

} // namespace basalt::server::runtime::player
