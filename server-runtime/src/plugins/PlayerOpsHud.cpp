#include <basalt/server/registry/Items.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerParticles.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/player/PlayerSounds.hpp>
#include <basalt/server/runtime/player/PlayerTabList.hpp>
#include <basalt/server/runtime/plugins/PlayerOps.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/wire/text/MiniMessage.hpp>

#include <algorithm>
#include <array>
#include <cmath>

namespace basalt::server::runtime::plugins::playerOps {
namespace {

namespace play = wire::protocol::play;

constexpr std::array<std::string_view, 11> kCategories{"master",  "music",  "record",  "weather", "block", "hostile",
                                                       "neutral", "player", "ambient", "voice",   "ui"};

[[nodiscard]] std::optional<play::SoundSource> categoryOf(const Value& value) {
    const std::string name = asText(value);
    if (name.empty()) {
        return std::nullopt;
    }
    for (std::size_t index = 0; index < kCategories.size(); ++index) {
        if (kCategories[index] == name || (name == "records" && index == 2) || (name == "players" && index == 7)) {
            return static_cast<play::SoundSource>(index);
        }
    }
    return play::SoundSource::Master;
}

[[nodiscard]] wire::text::Component textOf(const Value& value) { return wire::text::miniMessage::parse(asText(value)); }

[[nodiscard]] std::optional<play::ParticleOptions> optionsFor(const server::registry::ParticleType& type,
                                                              const Args& args, const std::size_t from) {
    using Shape = play::ParticleOptionShape;
    using Options = play::ParticleOptions;
    const auto number = [&](const std::size_t at) { return asReal(arg(args, from + at)); };
    const auto whole = [&](const std::size_t at) { return static_cast<std::int32_t>(asInt(arg(args, from + at))); };
    switch (type.optionShape) {
    case Shape::BlockState:
        return Options{Options::BlockState{whole(0)}};
    case Shape::Color:
        return Options{Options::Color{whole(0)}};
    case Shape::Spell:
        return Options{Options::Spell{whole(0), static_cast<float>(number(1))}};
    case Shape::Dust:
        return Options{Options::Dust{whole(0), static_cast<float>(number(1))}};
    case Shape::DustTransition:
        return Options{Options::DustTransition{whole(0), whole(1), static_cast<float>(number(2))}};
    case Shape::Number:
        return Options{Options::Number{static_cast<float>(number(0))}};
    case Shape::Delay:
        return Options{Options::Delay{whole(0)}};
    case Shape::Geyser:
        return Options{Options::Geyser{whole(0)}};
    case Shape::GeyserBase:
        return Options{Options::GeyserBase{whole(0), static_cast<float>(number(1))}};
    case Shape::Trail:
        return Options{Options::Trail{number(0), number(1), number(2), whole(3), whole(4)}};
    case Shape::Item: {
        const auto item = wire::common::Identifier::parse(asText(arg(args, from)));
        const server::registry::Item* found =
            item.has_value() ? server::registry::items::registry().byName(*item) : nullptr;
        return found == nullptr ? std::nullopt : std::optional{Options{Options::Item{found->protocolId, 1, {}}}};
    }
    case Shape::None:
    case Shape::Vibration:
        return std::nullopt;
    }
    return std::nullopt;
}

} // namespace

std::optional<Value> hud(Context& context, const Verb verb, const std::string_view name, const Args& args) {
    player::PlayerSession& session = context.session;
    player::PlayerExtras& extras = session.extras;
    const player::PlayerConnection& connection = session.connection();
    const Value& first = arg(args, 0);
    if (name == "title_part") {
        if (!connection.sendTitlePiece) {
            return false;
        }
        const std::string kind = asText(first);
        const player::TitlePiece piece = kind == "subtitle"    ? player::TitlePiece::Subtitle
                                         : kind == "actionbar" ? player::TitlePiece::ActionBar
                                                               : player::TitlePiece::Title;
        connection.sendTitlePiece(piece, textOf(arg(args, 1)), 0, 0, 0);
        return true;
    }
    if (name == "title_times") {
        if (!connection.sendTitlePiece) {
            return false;
        }
        connection.sendTitlePiece(
            player::TitlePiece::Times, wire::text::Component{}, static_cast<std::int32_t>(asInt(first)),
            static_cast<std::int32_t>(asInt(arg(args, 1))), static_cast<std::int32_t>(asInt(arg(args, 2))));
        return true;
    }
    if (name == "clear_title") {
        if (!connection.sendTitlePiece) {
            return false;
        }
        connection.sendTitlePiece(asBool(first) ? player::TitlePiece::Reset : player::TitlePiece::Clear,
                                  wire::text::Component{}, 0, 0, 0);
        return true;
    }
    if (name == "tab_header" || name == "tab_footer") {
        std::optional<std::string>& slot = name == "tab_header" ? extras.tabHeader : extras.tabFooter;
        if (verb == Verb::Get) {
            return slot.has_value() ? Value{*slot} : Value{};
        }
        slot = isNone(first) ? std::nullopt : std::optional{asText(first)};
        player::sendTabHeaderFooter(session);
        return true;
    }
    if (name == "tab_list") {
        extras.tabHeader = isNone(first) ? std::nullopt : std::optional{asText(first)};
        extras.tabFooter = isNone(arg(args, 1)) ? std::nullopt : std::optional{asText(arg(args, 1))};
        player::sendTabHeaderFooter(session);
        return true;
    }
    if (name == "list_name") {
        if (verb == Verb::Get) {
            return extras.listName.has_value() ? Value{*extras.listName} : Value{};
        }
        extras.listName = isNone(first) ? std::nullopt : std::optional{asText(first)};
        player::announceListEntry(context.registry, session);
        return true;
    }
    if (name == "listed") {
        if (verb == Verb::Get) {
            return !extras.unlisted;
        }
        extras.unlisted = !asBool(first, true);
        player::announceListEntry(context.registry, session);
        return true;
    }
    if (name == "list_order") {
        if (verb == Verb::Get) {
            return static_cast<std::int64_t>(extras.listOrder);
        }
        extras.listOrder = static_cast<std::int32_t>(asInt(first));
        player::announceListEntry(context.registry, session);
        return true;
    }
    if (name == "sound_at") {
        const std::optional<play::SoundSource> source = categoryOf(arg(args, 1));
        player::playSoundFor(session, asText(first), source.value_or(play::SoundSource::Master), asReal(arg(args, 2)),
                             asReal(arg(args, 3)), asReal(arg(args, 4)), static_cast<float>(asReal(arg(args, 5), 1.0)),
                             static_cast<float>(asReal(arg(args, 6), 1.0)), asInt(arg(args, 7)));
        return true;
    }
    if (name == "sound_entity") {
        world::LoadedWorld* world = worldOf(context);
        const entity::ServerEntity* target =
            world == nullptr ? nullptr : world->state().entity(static_cast<std::int32_t>(asInt(arg(args, 2), -1)));
        const double x = target != nullptr ? target->x : session.x;
        const double y = target != nullptr ? target->y : session.y;
        const double z = target != nullptr ? target->z : session.z;
        const std::optional<play::SoundSource> source = categoryOf(arg(args, 1));
        player::playSoundFor(session, asText(first), source.value_or(play::SoundSource::Master), x, y, z,
                             static_cast<float>(asReal(arg(args, 3), 1.0)),
                             static_cast<float>(asReal(arg(args, 4), 1.0)), asInt(arg(args, 5)));
        return true;
    }
    if (name == "stop_sound") {
        if (!connection.stopSound) {
            return false;
        }
        std::optional<std::string> sound = isNone(first) ? std::nullopt : std::optional{asText(first)};
        connection.stopSound(sound, categoryOf(arg(args, 1)));
        return true;
    }
    if (name == "particle_at") {
        const std::string particle = asText(first);
        const auto id = wire::common::Identifier::parse(
            particle.find(':') == std::string::npos ? "minecraft:" + particle : particle);
        const server::registry::ParticleType* type =
            id.has_value() ? server::registry::particleTypes::of(*id) : nullptr;
        if (type == nullptr) {
            return false;
        }
        play::LevelParticlesPacket packet{.particleTypeId = type->protocolId,
                                          .x = asReal(arg(args, 1)),
                                          .y = asReal(arg(args, 2)),
                                          .z = asReal(arg(args, 3)),
                                          .offsetX = static_cast<float>(asReal(arg(args, 5))),
                                          .offsetY = static_cast<float>(asReal(arg(args, 6))),
                                          .offsetZ = static_cast<float>(asReal(arg(args, 7))),
                                          .maxSpeedX = static_cast<float>(asReal(arg(args, 8))),
                                          .maxSpeedY = static_cast<float>(asReal(arg(args, 9))),
                                          .maxSpeedZ = static_cast<float>(asReal(arg(args, 10))),
                                          .count = static_cast<std::int32_t>(asInt(arg(args, 4), 1)),
                                          .overrideLimiter = asBool(arg(args, 11)),
                                          .randomization = static_cast<play::LevelParticlesPacket::Randomization>(
                                              std::clamp<std::int64_t>(asInt(arg(args, 12)), 0, 2))};
        if (type->needsOptions()) {
            packet.options = optionsFor(*type, args, 13);
            if (!packet.options.has_value()) {
                return false;
            }
        }
        player::showParticleFor(session, type->id.toString(), packet);
        return true;
    }
    if (name == "world_event") {
        if (!connection.sendLevelEvent) {
            return false;
        }
        connection.sendLevelEvent(
            static_cast<std::int32_t>(asInt(first)), static_cast<std::int32_t>(asInt(arg(args, 1))),
            static_cast<std::int32_t>(asInt(arg(args, 2))), static_cast<std::int32_t>(asInt(arg(args, 3))),
            static_cast<std::int32_t>(asInt(arg(args, 4))));
        return true;
    }
    if (name == "update_commands") {
        if (connection.sendCommandTree) {
            connection.sendCommandTree();
        }
        return true;
    }
    return std::nullopt;
}

} // namespace basalt::server::runtime::plugins::playerOps
