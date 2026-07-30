#include <basalt/server/runtime/EndFightBar.hpp>
#include <basalt/server/runtime/EndFightSystem.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/wire/common/json/Json.hpp>
#include <basalt/wire/text/ComponentJson.hpp>

namespace basalt::server::runtime {
namespace {

namespace play = wire::protocol::play;

void sendOperation(player::PlayerSession& player, const world::DragonFight& fight,
                   const play::BossEventPacket::Operation& operation) {
    if (player.connection().showBossBar) {
        const bool removing = std::holds_alternative<play::BossEventPacket::Remove>(operation);
        player.connection().showBossBar(fight.barId, endFightBar::nameOf(fight), fight.barProgress, !removing);
    } else if (player.connection().sendPlayPacket) {
        player.connection().sendPlayPacket(play::BossEventPacket::kId, [&](wire::io::ByteBuffer& buffer) {
            play::BossEventPacket{.id = fight.barId, .operation = operation}.write(buffer);
        });
    }
}

} // namespace

bool EndFightSystem::controlBar(world::LoadedWorld& world, const std::string_view action, const std::string_view text,
                                const std::int32_t value) {
    world::DragonFight* fight = world.dragonFight();
    if (fight == nullptr) {
        return false;
    }
    const auto toWatchers = [this, fight](const play::BossEventPacket::Operation& operation) {
        if (!fight->barVisible) {
            return;
        }
        for (player::PlayerSession* player : players_->snapshot()) {
            if (fight->barPlayers.contains(player->entityId())) {
                sendOperation(*player, *fight, operation);
            }
        }
    };
    if (action == "bar_title") {
        fight->barTitle = std::string{text};
        toWatchers(play::BossEventPacket::UpdateName{endFightBar::nameOf(*fight)});
    } else if (action == "bar_color" || action == "bar_style") {
        auto& field = action == "bar_color" ? fight->barColor : fight->barStyle;
        field = std::clamp(value, 0, action == "bar_color" ? 6 : 4);
        toWatchers(play::BossEventPacket::UpdateStyle{.color = static_cast<play::BossBarColor>(fight->barColor),
                                                      .overlay = static_cast<play::BossBarOverlay>(fight->barStyle)});
    } else if (action == "bar_flags") {
        fight->barFlags = value & (endFightBar::kDarkenSky | endFightBar::kPlayMusic | endFightBar::kCreateFog);
        toWatchers(play::BossEventPacket::UpdateProperties{
            .darkenScreen = (fight->barFlags & endFightBar::kDarkenSky) != 0,
            .playMusic = (fight->barFlags & endFightBar::kPlayMusic) != 0,
            .createWorldFog = (fight->barFlags & endFightBar::kCreateFog) != 0});
    } else if (action == "bar_add" || action == "bar_remove") {
        for (player::PlayerSession* player : players_->snapshot()) {
            if (player->entityId() != value) {
                continue;
            }
            const bool adding = action == "bar_add";
            if (adding == fight->barPlayers.contains(value)) {
                return true;
            }
            adding ? static_cast<void>(fight->barPlayers.insert(value))
                   : static_cast<void>(fight->barPlayers.erase(value));
            if (fight->barVisible) {
                sendOperation(*player, *fight,
                              adding ? play::BossEventPacket::Operation{endFightBar::addOf(*fight)}
                                     : play::BossEventPacket::Operation{play::BossEventPacket::Remove{}});
            }
            return true;
        }
        return false;
    } else if (action == "bar_clear") {
        toWatchers(play::BossEventPacket::Remove{});
        fight->barPlayers.clear();
    } else {
        return false;
    }
    return true;
}

} // namespace basalt::server::runtime

namespace basalt::server::runtime::endFightBar {

wire::text::Component nameOf(const world::DragonFight& fight) {
    if (fight.barTitle.empty()) {
        return wire::text::Component::translatable("entity.minecraft.ender_dragon");
    }
    const auto parsed = wire::common::parseJson(fight.barTitle);
    return parsed.has_value() ? wire::text::componentJson::deserialize(*parsed)
                              : wire::text::Component::text(fight.barTitle);
}

wire::protocol::play::BossEventPacket::Add addOf(const world::DragonFight& fight) {
    namespace play = wire::protocol::play;
    return play::BossEventPacket::Add{.name = nameOf(fight),
                                      .progress = fight.barProgress,
                                      .color = static_cast<play::BossBarColor>(fight.barColor),
                                      .overlay = static_cast<play::BossBarOverlay>(fight.barStyle),
                                      .darkenScreen = (fight.barFlags & kDarkenSky) != 0,
                                      .playMusic = (fight.barFlags & kPlayMusic) != 0,
                                      .createWorldFog = (fight.barFlags & kCreateFog) != 0};
}

} // namespace basalt::server::runtime::endFightBar
