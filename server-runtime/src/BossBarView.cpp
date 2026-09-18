#include <basalt/server/runtime/BossBarView.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>

#include <utility>

namespace basalt::server::runtime {
namespace {

namespace play = wire::protocol::play;

} // namespace

BossBarView::BossBarView(play::BossEventPacket::Add style) : style_(std::move(style)) {}

void BossBarView::send(player::PlayerRegistry& registry, std::int32_t playerId,
                       const play::BossEventPacket::Operation& operation) const {
    for (player::PlayerSession* player : registry.snapshot()) {
        if (player->entityId() != playerId) {
            continue;
        }
        if (player->connection().showBossBar) {
            const bool removed = std::holds_alternative<play::BossEventPacket::Remove>(operation);
            player->connection().showBossBar(id_, style_.name, style_.progress, !removed);
        } else if (player->connection().sendPlayPacket) {
            player->connection().sendPlayPacket(play::BossEventPacket::kId, [&](wire::io::ByteBuffer& buffer) {
                play::BossEventPacket{.id = id_, .operation = operation}.write(buffer);
            });
        }
    }
}

void BossBarView::track(player::PlayerRegistry& registry, const std::set<std::int32_t>& players) {
    for (const std::int32_t joined : players) {
        if (!players_.contains(joined)) {
            send(registry, joined, style_);
        }
    }
    for (const std::int32_t left : players_) {
        if (!players.contains(left)) {
            send(registry, left, play::BossEventPacket::Remove{});
        }
    }
    players_ = players;
}

void BossBarView::setProgress(player::PlayerRegistry& registry, float progress) {
    if (style_.progress == progress) {
        return;
    }
    style_.progress = progress;
    for (const std::int32_t shown : players_) {
        send(registry, shown, play::BossEventPacket::UpdateProgress{progress});
    }
}

void BossBarView::clear(player::PlayerRegistry& registry) { track(registry, {}); }

} // namespace basalt::server::runtime
