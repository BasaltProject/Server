#include <basalt/server/runtime/plugins/ServerBossBars.hpp>
#include <basalt/wire/common/json/Json.hpp>
#include <basalt/wire/text/ComponentJson.hpp>
#include <basalt/wire/text/MiniMessage.hpp>

#include <algorithm>
#include <array>
#include <cmath>

namespace basalt::server::runtime::plugins {
namespace {

namespace play = wire::protocol::play;

using command::commands::CustomBossBar;
using server::plugins::PluginBossBar;
using wire::common::Identifier;
using wire::text::Component;

constexpr std::array<std::pair<std::string_view, play::BossBarColor>, 7> kColors{
    {{"pink", play::BossBarColor::Pink},
     {"blue", play::BossBarColor::Blue},
     {"red", play::BossBarColor::Red},
     {"green", play::BossBarColor::Green},
     {"yellow", play::BossBarColor::Yellow},
     {"purple", play::BossBarColor::Purple},
     {"white", play::BossBarColor::White}}};

constexpr std::array<play::BossBarOverlay, 5> kOverlays{
    play::BossBarOverlay::Progress, play::BossBarOverlay::Notched6, play::BossBarOverlay::Notched10,
    play::BossBarOverlay::Notched12, play::BossBarOverlay::Notched20};

[[nodiscard]] Component fromJson(const std::string& text) {
    if (text.starts_with("mini:")) {
        return wire::text::miniMessage::parse(std::string_view{text}.substr(5));
    }
    const auto parsed = wire::common::parseJson(text);
    return parsed.has_value() ? wire::text::componentJson::deserialize(*parsed) : Component::text(text);
}

[[nodiscard]] std::string toJson(const Component& component) {
    return wire::common::writeJson(wire::text::componentJson::serialize(component));
}

[[nodiscard]] play::BossBarColor colorOf(std::string_view name) {
    for (const auto& [text, color] : kColors) {
        if (text == name) {
            return color;
        }
    }
    return play::BossBarColor::Purple;
}

[[nodiscard]] std::string colorName(play::BossBarColor color) {
    for (const auto& [text, each] : kColors) {
        if (each == color) {
            return std::string{text};
        }
    }
    return "purple";
}

[[nodiscard]] play::BossBarOverlay overlayOf(std::string_view name) {
    for (const play::BossBarOverlay overlay : kOverlays) {
        if (play::bossBarOverlayName(overlay) == name) {
            return overlay;
        }
    }
    return play::BossBarOverlay::Progress;
}

[[nodiscard]] play::BossEventPacket::Add addOf(const PluginBossBar& bar) {
    return play::BossEventPacket::Add{.name = fromJson(bar.title),
                                      .progress = static_cast<float>(bar.progress),
                                      .color = colorOf(bar.color),
                                      .overlay = overlayOf(bar.style),
                                      .darkenScreen = bar.darkenSky,
                                      .playMusic = bar.bossMusic,
                                      .createWorldFog = bar.fog};
}

[[nodiscard]] PluginBossBar packed(const CustomBossBar& bar) {
    PluginBossBar out;
    out.id = bar.id.toString();
    out.keyed = true;
    out.title = toJson(bar.name);
    out.color = colorName(bar.color);
    out.style = std::string{play::bossBarOverlayName(bar.overlay)};
    out.darkenSky = bar.darkenScreen;
    out.bossMusic = bar.playMusic;
    out.fog = bar.createFog;
    out.progress = static_cast<double>(bar.progress());
    out.visible = bar.visible;
    return out;
}

void store(const PluginBossBar& from, CustomBossBar& bar) {
    bar.name = fromJson(from.title);
    bar.color = colorOf(from.color);
    bar.overlay = overlayOf(from.style);
    bar.darkenScreen = from.darkenSky;
    bar.playMusic = from.bossMusic;
    bar.createFog = from.fog;
    bar.visible = from.visible;
    bar.max = std::max(bar.max, 1);
    bar.value = static_cast<std::int32_t>(std::lround(std::clamp(from.progress, 0.0, 1.0) * bar.max));
}

} // namespace

wire::common::Uuid ServerBossBars::uuidOf(std::string_view id) const {
    if (const auto found = transient_.find(id); found != transient_.end()) {
        return found->second.uuid;
    }
    if (const auto parsed = Identifier::parse(std::string{id}); parsed.has_value() && hooks_.registry != nullptr) {
        if (const CustomBossBar* bar = hooks_.registry->get(*parsed); bar != nullptr) {
            return bar->uuid;
        }
    }
    return {};
}

std::vector<player::PlayerSession*> ServerBossBars::watching(std::string_view id) const {
    std::vector<player::PlayerSession*> found;
    if (!hooks_.sessions) {
        return found;
    }
    const auto own = transient_.find(id);
    const CustomBossBar* kept = nullptr;
    if (own == transient_.end() && hooks_.registry != nullptr) {
        if (const auto parsed = Identifier::parse(std::string{id}); parsed.has_value()) {
            kept = hooks_.registry->get(*parsed);
        }
    }
    for (player::PlayerSession* session : hooks_.sessions()) {
        const bool inside = own != transient_.end() ? own->second.players.contains(session->uuid().toString())
                                                    : kept != nullptr && kept->players.contains(session->name());
        if (inside) {
            found.push_back(session);
        }
    }
    return found;
}

void ServerBossBars::sendTo(const player::PlayerSession& player, const wire::common::Uuid& barId,
                            const play::BossEventPacket::Operation& operation) const {
    if (!player.connection().sendPlayPacket) {
        return;
    }
    const play::BossEventPacket packet{.id = barId, .operation = operation};
    player.connection().sendPlayPacket(play::BossEventPacket::kId,
                                       [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
}

std::optional<std::string> ServerBossBars::create(const PluginBossBar& bar) {
    if (bar.keyed) {
        const auto parsed = Identifier::parse(bar.id);
        if (hooks_.registry == nullptr || !parsed.has_value() || hooks_.registry->get(*parsed) != nullptr) {
            return std::nullopt;
        }
        CustomBossBar made{.id = *parsed};
        store(bar, made);
        made.max = 100;
        made.value = static_cast<std::int32_t>(std::lround(std::clamp(bar.progress, 0.0, 1.0) * made.max));
        hooks_.registry->add(std::move(made));
        return parsed->toString();
    }
    Transient made;
    made.bar = bar;
    made.bar.keyed = false;
    made.bar.id = made.uuid.toString();
    const std::string id = made.bar.id;
    transient_.emplace(id, std::move(made));
    return id;
}

std::optional<PluginBossBar> ServerBossBars::get(std::string_view id) {
    if (const auto found = transient_.find(id); found != transient_.end()) {
        return found->second.bar;
    }
    if (const auto parsed = Identifier::parse(std::string{id}); parsed.has_value() && hooks_.registry != nullptr) {
        if (const CustomBossBar* bar = hooks_.registry->get(*parsed); bar != nullptr) {
            return packed(*bar);
        }
    }
    return std::nullopt;
}

std::vector<std::string> ServerBossBars::kept() {
    std::vector<std::string> ids;
    if (hooks_.registry != nullptr) {
        for (const CustomBossBar* bar : std::as_const(*hooks_.registry).all()) {
            ids.push_back(bar->id.toString());
        }
    }
    return ids;
}

bool ServerBossBars::update(const PluginBossBar& next) {
    const std::optional<PluginBossBar> before = get(next.id);
    if (!before.has_value()) {
        return false;
    }
    PluginBossBar after = next;
    after.keyed = before->keyed;
    if (const auto found = transient_.find(after.id); found != transient_.end()) {
        found->second.bar = after;
    } else if (const auto parsed = Identifier::parse(after.id); parsed.has_value()) {
        store(after, *hooks_.registry->get(*parsed));
        after = packed(*hooks_.registry->get(*parsed));
    }
    const wire::common::Uuid barId = uuidOf(after.id);
    for (const player::PlayerSession* session : watching(after.id)) {
        if (!before->visible && !after.visible) {
            continue;
        }
        if (before->visible && !after.visible) {
            sendTo(*session, barId, play::BossEventPacket::Remove{});
            continue;
        }
        if (!before->visible) {
            sendTo(*session, barId, addOf(after));
            continue;
        }
        if (before->title != after.title) {
            sendTo(*session, barId, play::BossEventPacket::UpdateName{fromJson(after.title)});
        }
        if (std::abs(before->progress - after.progress) > 1e-9) {
            sendTo(*session, barId, play::BossEventPacket::UpdateProgress{static_cast<float>(after.progress)});
        }
        if (before->color != after.color || before->style != after.style) {
            sendTo(*session, barId, play::BossEventPacket::UpdateStyle{colorOf(after.color), overlayOf(after.style)});
        }
        if (before->darkenSky != after.darkenSky || before->bossMusic != after.bossMusic || before->fog != after.fog) {
            sendTo(*session, barId,
                   play::BossEventPacket::UpdateProperties{after.darkenSky, after.bossMusic, after.fog});
        }
    }
    return true;
}

bool ServerBossBars::remove(std::string_view id) {
    if (!get(id).has_value()) {
        return false;
    }
    clearPlayers(id);
    if (const auto found = transient_.find(id); found != transient_.end()) {
        transient_.erase(found);
    } else if (const auto parsed = Identifier::parse(std::string{id}); parsed.has_value()) {
        hooks_.registry->remove(*parsed);
    }
    return true;
}

bool ServerBossBars::addPlayer(std::string_view id, std::string_view uuid) {
    const std::optional<PluginBossBar> bar = get(id);
    if (!bar.has_value() || !hooks_.sessions) {
        return false;
    }
    for (const player::PlayerSession* session : hooks_.sessions()) {
        if (session->uuid().toString() != uuid) {
            continue;
        }
        bool added = false;
        if (const auto found = transient_.find(id); found != transient_.end()) {
            added = found->second.players.insert(std::string{uuid}).second;
        } else if (const auto parsed = Identifier::parse(std::string{id}); parsed.has_value()) {
            added = hooks_.registry->get(*parsed)->players.insert(session->name()).second;
        }
        if (added && bar->visible) {
            sendTo(*session, uuidOf(id), addOf(*bar));
        }
        return true;
    }
    return false;
}

bool ServerBossBars::removePlayer(std::string_view id, std::string_view uuid) {
    if (!get(id).has_value() || !hooks_.sessions) {
        return false;
    }
    for (const player::PlayerSession* session : hooks_.sessions()) {
        if (session->uuid().toString() != uuid) {
            continue;
        }
        bool removed = false;
        if (const auto found = transient_.find(id); found != transient_.end()) {
            removed = found->second.players.erase(std::string{uuid}) > 0;
        } else if (const auto parsed = Identifier::parse(std::string{id}); parsed.has_value()) {
            removed = hooks_.registry->get(*parsed)->players.erase(session->name()) > 0;
        }
        if (removed) {
            sendTo(*session, uuidOf(id), play::BossEventPacket::Remove{});
        }
        return removed;
    }
    return false;
}

void ServerBossBars::clearPlayers(std::string_view id) {
    const wire::common::Uuid barId = uuidOf(id);
    for (const player::PlayerSession* session : watching(id)) {
        sendTo(*session, barId, play::BossEventPacket::Remove{});
    }
    if (const auto found = transient_.find(id); found != transient_.end()) {
        found->second.players.clear();
    } else if (const auto parsed = Identifier::parse(std::string{id});
               parsed.has_value() && hooks_.registry != nullptr) {
        if (CustomBossBar* bar = hooks_.registry->get(*parsed); bar != nullptr) {
            bar->players.clear();
        }
    }
}

void ServerBossBars::left(const std::string& uuid) {
    for (auto& [id, bar] : transient_) {
        bar.players.erase(uuid);
    }
}

std::vector<std::string> ServerBossBars::players(std::string_view id) {
    std::vector<std::string> found;
    for (const player::PlayerSession* session : watching(id)) {
        found.push_back(session->uuid().toString());
    }
    return found;
}

} // namespace basalt::server::runtime::plugins
