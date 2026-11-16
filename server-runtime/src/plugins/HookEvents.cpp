#include <basalt/server/plugins/PluginEventTypes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/entity/Breeding.hpp>
#include <basalt/server/runtime/entity/GeneratedEntityPresentation.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/plugins/HookEvents.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>

#include <algorithm>
#include <expected>
#include <map>
#include <optional>

namespace basalt::server::runtime::plugins::hookEvents {
namespace {

[[nodiscard]] std::int32_t cauldronLevel(const std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr || block->path() == "cauldron") {
        return 0;
    }
    const auto properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto level = properties.find("level");
    return level == properties.end() ? 3 : std::stoi(level->second);
}

[[nodiscard]] std::string cauldronBlockData(const std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    std::string text = "minecraft:" + std::string{block == nullptr ? "cauldron" : block->path()};
    const auto properties = registry::blockStateDefinitions::propertiesOf(stateId);
    if (const auto level = properties.find("level"); level != properties.end()) {
        text += "[level=" + level->second + "]";
    }
    return text;
}

[[nodiscard]] std::optional<std::int32_t> cauldronStateOf(const std::string_view data) {
    const std::size_t bracket = data.find('[');
    const std::expected<wire::common::Identifier, wire::common::IdentifierError> id =
        wire::common::Identifier::parse(data.substr(0, bracket));
    if (!id.has_value() || !id->path().ends_with("cauldron")) {
        return std::nullopt;
    }
    std::map<std::string, std::string> properties;
    if (bracket != std::string_view::npos) {
        const std::string_view body = data.substr(bracket + 1, data.size() - bracket - 2);
        const std::size_t equals = body.find('=');
        if (equals != std::string_view::npos) {
            properties.emplace(std::string{body.substr(0, equals)}, std::string{body.substr(equals + 1)});
        }
    }
    return registry::blockStateDefinitions::stateIdOf(*id, properties);
}

/// The `org.bukkit.entity.Pose` constant of each pose.
[[nodiscard]] std::string_view poseName(const wire::protocol::play::Pose pose) {
    using wire::protocol::play::Pose;
    switch (pose) {
    case Pose::Standing:
        return "STANDING";
    case Pose::FallFlying:
        return "FALL_FLYING";
    case Pose::Sleeping:
        return "SLEEPING";
    case Pose::Swimming:
        return "SWIMMING";
    case Pose::SpinAttack:
        return "SPIN_ATTACK";
    case Pose::Crouching:
        return "SNEAKING";
    case Pose::LongJumping:
        return "LONG_JUMPING";
    case Pose::Dying:
        return "DYING";
    case Pose::Croaking:
        return "CROAKING";
    case Pose::UsingTongue:
        return "USING_TONGUE";
    case Pose::Sitting:
        return "SITTING";
    case Pose::Roaring:
        return "ROARING";
    case Pose::Sniffing:
        return "SNIFFING";
    case Pose::Emerging:
        return "EMERGING";
    case Pose::Digging:
        return "DIGGING";
    case Pose::Sliding:
        return "SLIDING";
    case Pose::Shooting:
        return "SHOOTING";
    case Pose::Inhaling:
        return "INHALING";
    }
    return "STANDING";
}

} // namespace

void poseChange(const EventSink& sink, const std::string_view world, const std::int32_t id, const std::string_view type,
                const double x, const double y, const double z, const wire::protocol::play::Pose from,
                const wire::protocol::play::Pose to) {
    static_cast<void>(
        sink.allowed(&server::plugins::pluginEventTypes::entityPoseChange, [&](server::plugins::PluginEvent& event) {
            entity(event, world, id, type, x, y, z);
            event.set("pose", std::string{poseName(to)});
            event.set("old_pose", std::string{poseName(from)});
        }));
}

void watchPose(const EventSink& sink, const std::string_view world, entity::ServerEntity& mob) {
    const bool shown = mob.goat != nullptr || mob.breeze != nullptr || mob.camel != nullptr || mob.frog != nullptr ||
                       mob.forcedPose != -1 || (mob.forcedFlags & 0x92) != 0 ||
                       (mob.typeTraits() & entity::traits::kWarden) != 0;
    if (!shown && mob.lastPose <= 0) {
        return;
    }
    using wire::protocol::play::Pose;
    const Pose now = entity::entityPose(mob).value_or(Pose::Standing);
    const std::int8_t before = mob.lastPose < 0 ? static_cast<std::int8_t>(Pose::Standing) : mob.lastPose;
    mob.lastPose = static_cast<std::int8_t>(now);
    if (before != mob.lastPose) {
        poseChange(sink, world, mob.entityId(), mob.type().toString(), mob.x, mob.y, mob.z, static_cast<Pose>(before),
                   now);
    }
}

void entity(server::plugins::PluginEvent& event, const std::string_view world, const entity::ServerEntity& who) {
    event.set("entity_id", static_cast<std::int64_t>(who.entityId()));
    event.set("entity_type", who.type().toString());
    event.set("world", std::string{world});
    event.set("x", who.x);
    event.set("y", who.y);
    event.set("z", who.z);
}

void entity(server::plugins::PluginEvent& event, const std::string_view world, const std::int32_t id,
            const std::string_view type, const double x, const double y, const double z) {
    event.set("entity_id", static_cast<std::int64_t>(id));
    event.set("entity_type", std::string{type});
    event.set("world", std::string{world});
    event.set("x", x);
    event.set("y", y);
    event.set("z", z);
}

void position(server::plugins::PluginEvent& event, const std::string_view world, const std::int32_t x,
              const std::int32_t y, const std::int32_t z) {
    event.set("world", std::string{world});
    event.set("x", static_cast<std::int64_t>(x));
    event.set("y", static_cast<std::int64_t>(y));
    event.set("z", static_cast<std::int64_t>(z));
}

bool changeBlockAllowed(const EventSink& sink, const std::string_view world, const entity::ServerEntity& who,
                        const std::int32_t x, const std::int32_t y, const std::int32_t z, const std::int32_t fromState,
                        const std::int32_t toState, const std::string_view cause) {
    return sink.allowed(&server::plugins::pluginEventTypes::entityChangeBlock,
                        [&](server::plugins::PluginEvent& event) {
                            event.set("entity_id", static_cast<std::int64_t>(who.entityId()));
                            event.set("entity_type", who.type().toString());
                            position(event, world, x, y, z);
                            event.set("block", blockName(fromState));
                            event.set("to_block", blockName(toState));
                            event.set("cause", std::string{cause});
                        });
}

bool combustAllowed(const EventSink& sink, const std::string_view world, const std::int32_t id,
                    const std::string_view type, std::int32_t& ticks, const std::string_view cause,
                    const Igniter& igniter) {
    const server::plugins::PluginEvent* answered =
        sink.ask(&server::plugins::pluginEventTypes::entityCombust, [&](server::plugins::PluginEvent& event) {
            event.set("entity_id", static_cast<std::int64_t>(id));
            event.set("entity_type", std::string{type});
            event.set("world", std::string{world});
            event.set("ticks", static_cast<std::int64_t>(ticks));
            event.set("cause", std::string{cause});
            event.set("source_kind", std::string{igniter.block ? "block" : igniter.entityId >= 0 ? "entity" : ""});
            event.set("source_id", static_cast<std::int64_t>(igniter.entityId));
            event.set("source_x", static_cast<std::int64_t>(igniter.x));
            event.set("source_y", static_cast<std::int64_t>(igniter.y));
            event.set("source_z", static_cast<std::int64_t>(igniter.z));
        });
    if (answered == nullptr) {
        return true;
    }
    if (answered->cancelled()) {
        return false;
    }
    if (const auto* edited = std::get_if<std::int64_t>(&answered->get("ticks")); edited != nullptr) {
        ticks = static_cast<std::int32_t>(std::max<std::int64_t>(0, *edited));
    }
    return true;
}

void fillKnock(server::plugins::PluginEvent& event, const entity::entityCombat::Knock& knock) {
    event.set("cause", std::string{knock.cause});
    event.set("source_id", static_cast<std::int64_t>(knock.sourceId.value_or(-1)));
    event.set("power", knock.power);
    event.set("dir_x", knock.towardsX);
    event.set("dir_z", knock.towardsZ);
    event.set("kx", knock.delta[0]);
    event.set("ky", knock.delta[1]);
    event.set("kz", knock.delta[2]);
}

bool readKnock(const server::plugins::PluginEvent* answered, entity::entityCombat::Knock& knock) {
    if (answered == nullptr) {
        return true;
    }
    if (answered->cancelled()) {
        return false;
    }
    const auto read = [answered](const char* key, double& into) {
        const auto* edited = std::get_if<double>(&answered->get(key));
        if (edited == nullptr || *edited == into) {
            return false;
        }
        into = *edited;
        return true;
    };
    const int vectorEdits = static_cast<int>(read("kx", knock.delta[0])) +
                            static_cast<int>(read("ky", knock.delta[1])) + static_cast<int>(read("kz", knock.delta[2]));
    const int aimEdits = static_cast<int>(read("power", knock.power)) +
                         static_cast<int>(read("dir_x", knock.towardsX)) +
                         static_cast<int>(read("dir_z", knock.towardsZ));
    const bool vector = vectorEdits > 0;
    const bool aimed = aimEdits > 0;
    knock.recompute = aimed && !vector;
    return true;
}

bool spawnChangeAllowed(const EventSink& sink, const player::PlayerSession& who, std::int32_t& x, std::int32_t& y,
                        std::int32_t& z, const std::string_view cause) {
    const server::plugins::PluginEvent* answered =
        sink.ask(&server::plugins::pluginEventTypes::playerSpawnChange, [&](server::plugins::PluginEvent& event) {
            EventSink::player(event, who);
            event.set("x", static_cast<std::int64_t>(x));
            event.set("y", static_cast<std::int64_t>(y));
            event.set("z", static_cast<std::int64_t>(z));
            event.set("cause", std::string{cause});
        });
    if (answered == nullptr) {
        return true;
    }
    if (answered->cancelled()) {
        return false;
    }
    const auto read = [answered](const char* key, std::int32_t& into) {
        if (const auto* edited = std::get_if<std::int64_t>(&answered->get(key)); edited != nullptr) {
            into = static_cast<std::int32_t>(*edited);
        }
    };
    read("x", x);
    read("y", y);
    read("z", z);
    return true;
}

bool enterLove(const EventSink& sink, const std::string_view world, entity::ServerEntity& animal,
               const player::PlayerSession* cause) {
    std::int32_t ticks = entity::breeding::kLoveTicks;
    const server::plugins::PluginEvent* answered =
        sink.ask(&server::plugins::pluginEventTypes::entityEnterLove, [&](server::plugins::PluginEvent& event) {
            entity(event, world, animal);
            event.set("player_name", cause == nullptr ? std::string{} : cause->name());
            event.set("player_uuid", cause == nullptr ? std::string{} : cause->uuid().toString());
            event.set("ticks", static_cast<std::int64_t>(ticks));
        });
    if (answered != nullptr) {
        if (answered->cancelled()) {
            return false;
        }
        if (const auto* edited = std::get_if<std::int64_t>(&answered->get("ticks")); edited != nullptr) {
            ticks = static_cast<std::int32_t>(*edited);
        }
    }
    entity::breeding::feed(animal, cause == nullptr ? std::nullopt : std::optional{cause->uuid().toString()});
    animal.loveTicks = ticks;
    return true;
}

void eggHatch(const EventSink& sink, const std::string_view world, const entity::ServerEntity& egg, bool& hatching,
              std::int32_t& count, std::string& type) {
    const server::plugins::PluginEvent* answered =
        sink.ask(&server::plugins::pluginEventTypes::entityEggHatch, [&](server::plugins::PluginEvent& event) {
            entity(event, world, egg);
            event.set("hatching", hatching);
            event.set("count", static_cast<std::int64_t>(count));
            event.set("hatch_type", type);
        });
    if (answered == nullptr) {
        return;
    }
    if (const auto* edited = std::get_if<bool>(&answered->get("hatching")); edited != nullptr) {
        hatching = *edited;
    }
    if (const auto* edited = std::get_if<std::int64_t>(&answered->get("count")); edited != nullptr) {
        count = static_cast<std::int32_t>(std::clamp<std::int64_t>(*edited, 0, 127));
    }
    if (const auto* edited = std::get_if<std::string>(&answered->get("hatch_type")); edited != nullptr) {
        type = *edited;
    }
    if (!hatching) {
        count = 0;
    }
}

bool signOpenAllowed(const EventSink& sink, const player::PlayerSession& who, const std::int32_t x,
                     const std::int32_t y, const std::int32_t z, const bool front, const std::string_view cause) {
    return sink.allowed(&server::plugins::pluginEventTypes::playerSignOpen, [&](server::plugins::PluginEvent& event) {
        EventSink::player(event, who);
        event.set("x", static_cast<std::int64_t>(x));
        event.set("y", static_cast<std::int64_t>(y));
        event.set("z", static_cast<std::int64_t>(z));
        event.set("side", std::string{front ? "front" : "back"});
        event.set("cause", std::string{cause});
    });
}

void dropFor(world::LoadedWorld& world, const entity::ServerEntity& who, world::DroppedItem dropped) {
    const std::int32_t id = dropped.entityId;
    const std::string item = dropped.item.toString();
    const std::int64_t count = dropped.count;
    world.state().spawnItem(std::move(dropped));
    const bool kept = world.events().allowed(&server::plugins::pluginEventTypes::entityDropItem,
                                             [&](server::plugins::PluginEvent& event) {
                                                 entity(event, world.name(), who);
                                                 event.set("item_id", static_cast<std::int64_t>(id));
                                                 event.set("item", item);
                                                 event.set("count", count);
                                             });
    if (!kept) {
        static_cast<void>(world.state().removeItem(id));
    }
}

std::string blockName(const std::int32_t stateId) {
    const wire::common::Identifier* block = server::registry::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string{"minecraft:air"} : block->toString();
}

bool cauldronChange(const EventSink& sink, const std::string_view world, const std::int32_t x, const std::int32_t y,
                    const std::int32_t z, const std::string_view reason, const std::int32_t from, std::int32_t& to,
                    const player::PlayerSession* who, const std::int32_t entityId) {
    const server::plugins::PluginEvent* answered =
        sink.ask(&server::plugins::pluginEventTypes::blockCauldronLevel, [&](server::plugins::PluginEvent& event) {
            if (who != nullptr) {
                EventSink::player(event, *who);
            } else {
                event.set("player_name", std::string{});
                event.set("player_uuid", std::string{});
            }
            position(event, world, x, y, z);
            event.set("entity_id", static_cast<std::int64_t>(entityId));
            event.set("reason", std::string{reason});
            event.set("old_level", static_cast<std::int64_t>(cauldronLevel(from)));
            event.set("new_level", static_cast<std::int64_t>(cauldronLevel(to)));
            event.set("new_block", cauldronBlockData(to));
        });
    if (answered == nullptr) {
        return true;
    }
    if (answered->cancelled()) {
        return false;
    }
    if (const auto* edited = std::get_if<std::string>(&answered->get("new_block")); edited != nullptr) {
        if (const std::optional<std::int32_t> state = cauldronStateOf(*edited); state.has_value()) {
            to = *state;
        }
    }
    return true;
}

} // namespace basalt::server::runtime::plugins::hookEvents
