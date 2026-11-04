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

void playLevelUpSoundIfDue(PlayerSession& player, const std::span<PlayerSession* const> watchers,
                           const std::int64_t seed) {
    const std::optional<std::int32_t> level = player.experience.takeLevelUp();
    if (!level.has_value() || player.lastLevelUpSoundTick > player.aliveTicks - 100) {
        return;
    }
    player.lastLevelUpSoundTick = player.aliveTicks;
    const float volume = (*level > 30 ? 1.0F : static_cast<float>(*level) / 30.0F) * 0.75F;
    bool sentToSelf = false;
    for (const PlayerSession* watcher : watchers) {
        if (watcher == nullptr || watcher->worldName != player.worldName) {
            continue;
        }
        sentToSelf = sentToSelf || watcher->entityId() == player.entityId();
        playSoundFor(*watcher, "entity.player.levelup", wire::protocol::play::SoundSource::Players, player.x, player.y,
                     player.z, volume, 1.0F, seed);
    }
    if (!sentToSelf) {
        playSoundFor(player, "entity.player.levelup", wire::protocol::play::SoundSource::Players, player.x, player.y,
                     player.z, volume, 1.0F, seed);
    }
}

} // namespace basalt::server::runtime::player
