#include <basalt/server/plugins/PluginEventTypes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/entity/GeneratedEntityPresentation.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/plugins/HookEvents.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>

#include <algorithm>

namespace basalt::server::runtime::plugins::hookEvents {
namespace {

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

} // namespace basalt::server::runtime::plugins::hookEvents
