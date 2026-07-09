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

void BossBarView::apply(player::PlayerRegistry& registry) {
    std::set<std::int32_t> shown;
    if (visible_) {
        shown = inRange_;
        shown.insert(pinned_.begin(), pinned_.end());
        for (const std::int32_t away : sentAway_) {
            shown.erase(away);
        }
    }
    for (const std::int32_t joined : shown) {
        if (!players_.contains(joined)) {
            send(registry, joined, style_);
        }
    }
    for (const std::int32_t left : players_) {
        if (!shown.contains(left)) {
            send(registry, left, play::BossEventPacket::Remove{});
        }
    }
    players_ = std::move(shown);
}

void BossBarView::track(player::PlayerRegistry& registry, const std::set<std::int32_t>& players) {
    inRange_ = players;
    std::erase_if(sentAway_, [&players](std::int32_t id) { return !players.contains(id); });
    apply(registry);
}

void BossBarView::setName(player::PlayerRegistry& registry, wire::text::Component name) {
    style_.name = std::move(name);
    for (const std::int32_t shown : players_) {
        send(registry, shown, play::BossEventPacket::UpdateName{style_.name});
    }
}

void BossBarView::setLook(player::PlayerRegistry& registry, play::BossBarColor color, play::BossBarOverlay overlay) {
    style_.color = color;
    style_.overlay = overlay;
    for (const std::int32_t shown : players_) {
        send(registry, shown, play::BossEventPacket::UpdateStyle{color, overlay});
    }
}

void BossBarView::setFlags(player::PlayerRegistry& registry, bool darkenScreen, bool playMusic, bool createWorldFog) {
    style_.darkenScreen = darkenScreen;
    style_.playMusic = playMusic;
    style_.createWorldFog = createWorldFog;
    for (const std::int32_t shown : players_) {
        send(registry, shown, play::BossEventPacket::UpdateProperties{darkenScreen, playMusic, createWorldFog});
    }
}

void BossBarView::setVisible(player::PlayerRegistry& registry, bool visible) {
    visible_ = visible;
    apply(registry);
}

void BossBarView::pin(player::PlayerRegistry& registry, std::int32_t playerId, bool shown) {
    if (shown) {
        pinned_.insert(playerId);
        sentAway_.erase(playerId);
    } else {
        pinned_.erase(playerId);
        sentAway_.insert(playerId);
    }
    apply(registry);
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

void BossBarView::clear(player::PlayerRegistry& registry) {
    pinned_.clear();
    sentAway_.clear();
    track(registry, {});
}

} // namespace basalt::server::runtime
