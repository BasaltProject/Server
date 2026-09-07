#include "ExecuteCommand.hpp"

#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/command/ArgumentTypes.hpp>
#include <basalt/server/runtime/command/ExecuteArguments.hpp>
#include <basalt/server/runtime/command/RegistryArguments.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>

#include <cmath>
#include <cstdint>
#include <memory>
#include <set>

namespace basalt::server::runtime::command::commands {
namespace {

namespace reg = server::registry;

using executeCommand::blockPosOf;
using executeCommand::facing;
using executeCommand::kEyeHeight;
using executeCommand::targetsOf;
using executeCommand::wrapping;
using wire::common::Identifier;

/// parity: the eight relations `/execute on` accepts.
constexpr std::array<std::string_view, 8> kRelations{"attacker", "controller", "leasher", "origin",
                                                     "owner",    "passengers", "target",  "vehicle"};

using Sources = std::vector<std::shared_ptr<const CommandSource>>;

/// One source per target, which is what makes a clause fork.
[[nodiscard]] Sources perTarget(const CommandContext& context, std::string_view name,
                                const std::function<ExecutionSource::Changes(const CommandTarget&)>& change) {
    Sources built;
    for (const CommandTarget& target : targetsOf(context, name)) {
        built.push_back(wrapping(context.source(), change(target)));
    }
    return built;
}

/// parity: `positioned over`, which drops the position onto the named heightmap.
[[nodiscard]] Sources over(const CommandContext& context) {
    world::LoadedWorld* world = worldIn(context.source(), context.environment());
    const auto* heightmap = context.get<std::string>("heightmap");
    if (world == nullptr || heightmap == nullptr) {
        return {wrapping(context.source(), {})};
    }
    const std::array<double, 3> at = context.source().position();
    const auto blockX = static_cast<std::int32_t>(std::floor(at[0]));
    const auto blockZ = static_cast<std::int32_t>(std::floor(at[2]));
    const server::world::Chunk* chunk = world->state().chunkIfLoaded(blockX >> 4, blockZ >> 4);
    if (chunk == nullptr) {
        return {wrapping(context.source(), {})};
    }
    const server::world::HeightmapType type = *heightmap == "world_surface" ? server::world::HeightmapType::WorldSurface
                                              : *heightmap == "ocean_floor" ? server::world::HeightmapType::OceanFloor
                                              : *heightmap == "motion_blocking_no_leaves"
                                                  ? server::world::HeightmapType::MotionBlockingNoLeaves
                                                  : server::world::HeightmapType::MotionBlocking;
    const std::int32_t height = chunk->heightAt(blockX & 15, blockZ & 15, type);
    return {wrapping(context.source(), ExecutionSource::Changes{.position = std::array<double, 3>{
                                                                    at[0], static_cast<double>(height), at[2]}})};
}

} // namespace

void ExecuteCommand::buildContextClauses(const CommandNodeBuilder& root, CommandNode& execute) const {
    root.literal("as", [&execute](const CommandNodeBuilder& form) {
        form.argument("targets", entitiesArgument(), [&execute](const CommandNodeBuilder& targets) {
            targets.redirectTo(execute, true, [](const CommandContext& context) {
                return perTarget(context, "targets", [](const CommandTarget& target) {
                    return ExecutionSource::Changes{.player = target.player(), .entity = target};
                });
            });
        });
    });

    /// parity: `at`, which takes the target's position, rotation and world, but not its identity.
    root.literal("at", [&execute](const CommandNodeBuilder& form) {
        form.argument("targets", entitiesArgument(), [&execute](const CommandNodeBuilder& targets) {
            targets.redirectTo(execute, true, [](const CommandContext& context) {
                return perTarget(context, "targets", [](const CommandTarget& target) {
                    return ExecutionSource::Changes{.position =
                                                        std::array<double, 3>{target.x(), target.y(), target.z()},
                                                    .rotation = std::pair{target.yaw(), target.pitch()}};
                });
            });
        });
    });

    root.literal("positioned", [&execute](const CommandNodeBuilder& form) {
        form.argument("pos", vec3Argument(), [&execute](const CommandNodeBuilder& pos) {
            pos.redirectTo(execute, false, [](const CommandContext& context) -> Sources {
                const auto* coordinates = context.get<Coordinates>("pos");
                if (coordinates == nullptr) {
                    return {};
                }
                return {wrapping(context.source(), ExecutionSource::Changes{
                                                       .position = coordinates->resolve(context.source().position())})};
            });
        });
        form.literal("as", [&execute](const CommandNodeBuilder& as) {
            as.argument("targets", entitiesArgument(), [&execute](const CommandNodeBuilder& targets) {
                targets.redirectTo(execute, true, [](const CommandContext& context) {
                    return perTarget(context, "targets", [](const CommandTarget& target) {
                        return ExecutionSource::Changes{.position =
                                                            std::array<double, 3>{target.x(), target.y(), target.z()}};
                    });
                });
            });
        });
        form.literal("over", [&execute](const CommandNodeBuilder& form) {
            form.argument("heightmap", heightmapArgument(), [&execute](const CommandNodeBuilder& heightmap) {
                heightmap.redirectTo(execute, false, over);
            });
        });
    });

    root.literal("rotated", [&execute](const CommandNodeBuilder& form) {
        form.argument("rot", rotationArgument(), [&execute](const CommandNodeBuilder& rotation) {
            rotation.redirectTo(execute, false, [](const CommandContext& context) -> Sources {
                const auto* facingPair = context.get<std::pair<float, float>>("rot");
                if (facingPair == nullptr) {
                    return {};
                }
                return {wrapping(context.source(), ExecutionSource::Changes{.rotation = *facingPair})};
            });
        });
        form.literal("as", [&execute](const CommandNodeBuilder& as) {
            as.argument("targets", entitiesArgument(), [&execute](const CommandNodeBuilder& targets) {
                targets.redirectTo(execute, true, [](const CommandContext& context) {
                    return perTarget(context, "targets", [](const CommandTarget& target) {
                        return ExecutionSource::Changes{.rotation = std::pair{target.yaw(), target.pitch()}};
                    });
                });
            });
        });
    });

    root.literal("facing", [&execute](const CommandNodeBuilder& form) {
        form.argument("pos", vec3Argument(), [&execute](const CommandNodeBuilder& pos) {
            pos.redirectTo(execute, false, [](const CommandContext& context) -> Sources {
                const auto* coordinates = context.get<Coordinates>("pos");
                if (coordinates == nullptr) {
                    return {};
                }
                const std::array<double, 3> at = coordinates->resolve(context.source().position());
                return {facing(context.source(), at[0], at[1], at[2])};
            });
        });
        form.literal("entity", [&execute](const CommandNodeBuilder& entity) {
            entity.argument("targets", entitiesArgument(), [&execute](const CommandNodeBuilder& targets) {
                targets.argument("anchor", entityAnchorArgument(), [&execute](const CommandNodeBuilder& anchor) {
                    anchor.redirectTo(execute, true, [](const CommandContext& context) {
                        const auto* which = context.get<EntityAnchor>("anchor");
                        const bool eyes = which != nullptr && *which == EntityAnchor::Eyes;
                        Sources built;
                        for (const CommandTarget& target : targetsOf(context, "targets")) {
                            built.push_back(facing(context.source(), target.x(), target.y() + (eyes ? kEyeHeight : 0.0),
                                                   target.z()));
                        }
                        return built;
                    });
                });
            });
        });
    });

    root.literal("align", [&execute](const CommandNodeBuilder& form) {
        form.argument("axes", swizzleArgument(), [&execute](const CommandNodeBuilder& axes) {
            axes.redirectTo(execute, false, [](const CommandContext& context) -> Sources {
                const auto* wanted = context.get<std::set<char>>("axes");
                if (wanted == nullptr) {
                    return {};
                }
                const std::array<double, 3> at = context.source().position();
                return {wrapping(context.source(),
                                 ExecutionSource::Changes{.position = std::array<double, 3>{
                                                              wanted->contains('x') ? std::floor(at[0]) : at[0],
                                                              wanted->contains('y') ? std::floor(at[1]) : at[1],
                                                              wanted->contains('z') ? std::floor(at[2]) : at[2]}})};
            });
        });
    });

    root.literal("anchored", [&execute](const CommandNodeBuilder& form) {
        form.argument("anchor", entityAnchorArgument(), [&execute](const CommandNodeBuilder& anchor) {
            anchor.redirectTo(execute, false, [](const CommandContext& context) -> Sources {
                const auto* which = context.get<EntityAnchor>("anchor");
                return {
                    wrapping(context.source(),
                             ExecutionSource::Changes{.anchorEyes = which != nullptr && *which == EntityAnchor::Eyes})};
            });
        });
    });

    root.literal("in", [&execute](const CommandNodeBuilder& form) {
        form.argument("dimension", worldArgument(), [&execute](const CommandNodeBuilder& dimension) {
            dimension.redirectTo(execute, false, [](const CommandContext& context) -> Sources {
                auto* const* world = context.get<world::LoadedWorld*>("dimension");
                if (world == nullptr) {
                    return {};
                }
                return {wrapping(context.source(), ExecutionSource::Changes{.world = *world})};
            });
        });
    });

    /// parity: `execute summon`, which spawns the entity and runs the rest of the line as it.
    root.literal("summon", [this, &execute](const CommandNodeBuilder& form) {
        form.argument(
            "entity", resourceArgument("minecraft:entity_type"), [this, &execute](const CommandNodeBuilder& named) {
                named.suggestsFromClient(std::string{suggestionProviders::kSummonableEntities});
                named.redirectTo(execute, false, [this](const CommandContext& context) -> Sources {
                    player::PlayerRegistry* registry = hooks_.players;
                    const auto* id = context.get<Identifier>("entity");
                    if (registry == nullptr || id == nullptr || *id == reg::entityTypes::player()) {
                        return {};
                    }
                    const reg::EntityTypeEntry* type = reg::entityTypes::of(*id);
                    world::LoadedWorld* world = worldIn(context.source(), context.environment());
                    if (type == nullptr || world == nullptr) {
                        return {};
                    }
                    const std::array<double, 3> at = context.source().position();
                    entity::ServerEntity& spawned = world->state().spawnEntity(std::make_unique<entity::ServerEntity>(
                        registry->allocateEntityId(), wire::common::Uuid::random(), *id, type->protocolId, at[0], at[1],
                        at[2]));
                    return {wrapping(context.source(),
                                     ExecutionSource::Changes{.entity = CommandTarget{spawned, world->name()}})};
                });
            });
    });

    root.literal("on", [&execute](const CommandNodeBuilder& form) {
        // parity: the eight relations `/execute on` accepts. None of them exist in this build, so
        // each yields nobody and the rest of the line does not run.
        for (const std::string_view relation : kRelations) {
            form.literal(std::string{relation}, [&execute](const CommandNodeBuilder& one) {
                one.redirectTo(execute, true, [](const CommandContext&) { return Sources{}; });
            });
        }
    });
}

} // namespace basalt::server::runtime::command::commands
