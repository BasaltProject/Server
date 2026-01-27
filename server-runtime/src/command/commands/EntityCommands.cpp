#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/command/ArgumentTypes.hpp>
#include <basalt/server/runtime/command/CommandTarget.hpp>
#include <basalt/server/runtime/command/Coordinates.hpp>
#include <basalt/server/runtime/command/ExecuteArguments.hpp>
#include <basalt/server/runtime/command/RegistryArguments.hpp>
#include <basalt/server/runtime/command/WorldArguments.hpp>
#include <basalt/server/runtime/command/commands/EntityCommands.hpp>
#include <basalt/server/runtime/entity/EntityNbt.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/world/DroppedItemNbt.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/wire/common/json/Json.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>
#include <basalt/wire/text/ComponentJson.hpp>
#include <basalt/wire/text/Serializers.hpp>

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace basalt::server::runtime::command::commands {
namespace {

namespace play = wire::protocol::play;
namespace reg = server::registry;

using wire::common::Identifier;
using wire::text::Component;
using wire::text::kGray;

constexpr double kDegreesPerRadian = 57.2957795130823;
constexpr double kEyeHeight = 1.62;

[[nodiscard]] const std::vector<CommandTarget>& targetsOf(const CommandContext& context, std::string_view name) {
    static const std::vector<CommandTarget> none;
    const auto* found = context.get<std::vector<CommandTarget>>(name);
    return found == nullptr ? none : *found;
}

/// The one target a `single = true` selector matched, or null when it matched nobody.
[[nodiscard]] const CommandTarget* only(const CommandContext& context, std::string_view name) {
    const std::vector<CommandTarget>& matched = targetsOf(context, name);
    return matched.size() == 1 ? &matched.front() : nullptr;
}

void sendToWorld(const CommandContext& context, std::int32_t packetId,
                 const std::function<void(wire::io::ByteBuffer&)>& write) {
    const Parsed<world::LoadedWorld*> world = context.world();
    if (!world.has_value()) {
        return;
    }
    for (const player::PlayerSession* player : context.environment().playersIn(**world)) {
        if (player->connection().sendPlayPacket) {
            player->connection().sendPlayPacket(packetId, write);
        }
    }
}

/// `CustomName` is a text component; only its plain text survives into an entity name here.
[[nodiscard]] std::string plainName(std::string_view value) {
    const std::expected<wire::common::JsonValue, wire::common::JsonError> parsed = wire::common::parseJson(value);
    if (!parsed.has_value()) {
        return std::string{value};
    }
    return wire::text::toPlainText(wire::text::componentJson::deserialize(*parsed));
}

} // namespace

SummonCommand::SummonCommand(player::PlayerRegistry& players)
    : Command("summon", permissionLevel::kGamemasters), players_(&players) {}

void SummonCommand::build(const CommandNodeBuilder& root) const {
    /// The tag as vanilla writes it, so the decoder that reads a saved entity can read this one.
    const auto tagFor = [](const CommandContext& context, const wire::common::Identifier& id,
                           const std::array<double, 3>& at, bool withNbt) {
        wire::nbt::NbtCompound tag;
        if (withNbt) {
            if (const auto* given = context.get<wire::nbt::NbtCompound>("nbt"); given != nullptr) {
                tag = *given;
            }
        }
        tag.putString("id", id.toString());
        tag.set("Pos", wire::nbt::NbtTag::ofList(
                           wire::nbt::NbtList{wire::nbt::NbtType::Double,
                                              {wire::nbt::NbtTag::ofDouble(at[0]), wire::nbt::NbtTag::ofDouble(at[1]),
                                               wire::nbt::NbtTag::ofDouble(at[2])}}));
        return tag;
    };

    const auto summon = [this, tagFor](const CommandContext& context, bool positioned,
                                       bool withNbt) -> Parsed<std::int32_t> {
        const auto* id = context.get<Identifier>("entity");
        if (id == nullptr) {
            return 0;
        }
        if (*id == reg::entityTypes::player()) {
            return std::unexpected{
                CommandSyntaxError{.component = Component::translatable("commands.summon.failed.uuid")}};
        }
        const reg::EntityTypeEntry* type = reg::entityTypes::of(*id);
        if (type == nullptr) {
            return std::unexpected{CommandSyntaxError{.component = Component::translatable("commands.summon.failed")}};
        }
        const Parsed<world::LoadedWorld*> world = context.world();
        if (!world.has_value()) {
            return std::unexpected{world.error()};
        }
        const auto* coordinates = positioned ? context.get<Coordinates>("pos") : nullptr;
        const std::array<double, 3> at =
            coordinates == nullptr ? context.source().position() : coordinates->resolve(context.source().position());
        // parity: an item on the floor is an `ItemEntity`, not something generic. Here it is its own
        // kind, so summoning one has to build that kind: as a plain entity it never fell, never
        // merged and no plate or hopper saw it, because those all read the world's item list.
        // Measured in `placas`, where the jar's items landed on the plate and ours hung in the air.
        const wire::nbt::NbtCompound tag = tagFor(context, *id, at, withNbt);
        if (*id == wire::common::Identifier::vanilla("item")) {
            const std::optional<world::DroppedItem> dropped =
                world::droppedItemNbt::decode(tag, [this] { return players_->allocateEntityId(); });
            if (!dropped.has_value()) {
                return std::unexpected{
                    CommandSyntaxError{.component = Component::translatable("commands.summon.failed")}};
            }
            static_cast<void>((*world)->state().spawnItem(*dropped));
            context.success(Component::translatable("commands.summon.success", {Component::text(id->toString())}),
                            true);
            return 1;
        }
        // parity: `SummonCommand` builds the entity **through the save codec**
        // (`EntityType.loadEntityRecursive`), so every tag a saved entity understands works here too.
        // Doing it by hand read two fields out of thirty: measured in `flechavibra`, where an arrow
        // summoned with `Motion` simply hung in the air because nothing applied it.
        std::unique_ptr<entity::ServerEntity> entity =
            entity::entityNbt::decode(tag, players_->allocateEntityId(), type->protocolId);
        if (entity == nullptr) {
            return std::unexpected{CommandSyntaxError{.component = Component::translatable("commands.summon.failed")}};
        }
        // `CustomName` is a text component on the way in and plain text once it is an entity's name.
        if (const std::string_view named = tag.getString("CustomName"); !named.empty()) {
            entity->customName = plainName(named);
        }
        static_cast<void>((*world)->state().spawnEntity(std::move(entity)));
        context.success(Component::translatable("commands.summon.success", {Component::text(id->toString())}), true);
        return 1;
    };

    root.argument("entity", resourceArgument("minecraft:entity_type"), [summon](const CommandNodeBuilder& named) {
        named.suggestsFromClient(std::string{suggestionProviders::kSummonableEntities});
        named.executes([summon](const CommandContext& context) { return summon(context, false, false); });
        named.argument("pos", vec3Argument(), [summon](const CommandNodeBuilder& pos) {
            pos.executes([summon](const CommandContext& context) { return summon(context, true, false); });
            pos.argument("nbt", nbtCompoundArgument(), [summon](const CommandNodeBuilder& nbt) {
                nbt.executes([summon](const CommandContext& context) { return summon(context, true, true); });
            });
        });
    });
}

DamageCommand::DamageCommand(std::function<std::vector<Identifier>()> damageTypes)
    : Command("damage", permissionLevel::kGamemasters), damageTypes_(std::move(damageTypes)) {}

void DamageCommand::build(const CommandNodeBuilder& root) const {
    const auto damage = [](const CommandContext& context) -> Parsed<std::int32_t> {
        const CommandTarget* target = only(context, "target");
        const auto* amount = context.get<float>("amount");
        if (target == nullptr || amount == nullptr) {
            return 0;
        }
        if (player::PlayerSession* session = target->player(); session != nullptr) {
            const auto* type = context.get<Identifier>("damageType");
            if (session->connection().hurt) {
                session->connection().hurt(*amount, "generic", type == nullptr ? Identifier::vanilla("generic") : *type,
                                           std::nullopt);
            }
        } else if (entity::ServerEntity* what = target->entity(); what != nullptr) {
            what->health -= static_cast<double>(*amount);
            if (what->health <= 0.0) {
                what->killed = true;
                // parity: only a `LivingEntity` has a `tickDeath` to play out.
                (reg::entityAttributes::isLiving(what->type()) ? what->dying : what->removed) = true;
            }
        }
        // parity: an item entity takes damage from fire and explosions, neither of which this
        // reaches: `/damage` on one does nothing rather than pretending.
        context.success(Component::translatable("commands.damage.success", {Component::text(std::to_string(*amount)),
                                                                            Component::text(target->name())}),
                        true);
        return 1;
    };

    root.argument("target", entitiesArgument(true), [this, damage](const CommandNodeBuilder& target) {
        target.argument("amount", floatArgument(0.0F), [this, damage](const CommandNodeBuilder& amount) {
            amount.executes(damage);
            amount.argument(
                "damageType", resourceArgument("minecraft:damage_type"),
                [this, damage](const CommandNodeBuilder& type) {
                    type.suggestsFromServer([this](const SuggestionContext& context) {
                        return context.matchingIds(damageTypes_ ? damageTypes_() : std::vector<Identifier>{});
                    });
                    type.executes(damage);
                    // Where it came from and who dealt it. Neither changes the damage in
                    // this build, which has no knockback or attribution: they are
                    // accepted, not acted on.
                    type.literal("at", [damage](const CommandNodeBuilder& at) {
                        at.argument("location", vec3Argument(),
                                    [damage](const CommandNodeBuilder& location) { location.executes(damage); });
                    });
                    type.literal("by", [damage](const CommandNodeBuilder& by) {
                        by.argument("entity", entitiesArgument(true), [damage](const CommandNodeBuilder& entity) {
                            entity.executes(damage);
                            entity.literal("from", [damage](const CommandNodeBuilder& from) {
                                from.argument("cause", entitiesArgument(true),
                                              [damage](const CommandNodeBuilder& cause) { cause.executes(damage); });
                            });
                        });
                    });
                });
        });
    });
}

RideCommand::RideCommand() : Command("ride", permissionLevel::kGamemasters) {}

void RideCommand::build(const CommandNodeBuilder& root) const {
    root.argument("target", entitiesArgument(true), [](const CommandNodeBuilder& target) {
        target.literal("mount", [](const CommandNodeBuilder& mount) {
            mount.argument("vehicle", entitiesArgument(true), [](const CommandNodeBuilder& vehicle) {
                vehicle.executes([](const CommandContext& context) -> Parsed<std::int32_t> {
                    const CommandTarget* rider = only(context, "target");
                    const CommandTarget* onto = only(context, "vehicle");
                    if (rider == nullptr || onto == nullptr) {
                        return 0;
                    }
                    if (rider->entityId() == onto->entityId()) {
                        return std::unexpected{CommandSyntaxError{
                            .component = Component::translatable("commands.ride.mount.failure.loop")}};
                    }
                    if (entity::ServerEntity* vehicleEntity = onto->entity(); vehicleEntity != nullptr) {
                        vehicleEntity->passengerIds.push_back(rider->entityId());
                    }
                    if (entity::ServerEntity* riderEntity = rider->entity(); riderEntity != nullptr) {
                        riderEntity->vehicleId = onto->entityId();
                    }
                    const play::SetEntityLinkPacket packet{.sourceId = rider->entityId(),
                                                           .destinationId = onto->entityId()};
                    sendToWorld(context, play::SetEntityLinkPacket::kId,
                                [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
                    context.success(
                        Component::translatable("commands.ride.mount.success",
                                                {Component::text(rider->name()), Component::text(onto->name())}),
                        true);
                    return 1;
                });
            });
        });
        target.literal("dismount", [](const CommandNodeBuilder& dismount) {
            dismount.executes([](const CommandContext& context) -> Parsed<std::int32_t> {
                const CommandTarget* rider = only(context, "target");
                entity::ServerEntity* riding = rider == nullptr ? nullptr : rider->entity();
                if (rider == nullptr || riding == nullptr || !riding->vehicleId.has_value()) {
                    return std::unexpected{
                        CommandSyntaxError{.component = Component::translatable(
                                               "commands.ride.not_riding",
                                               {Component::text(rider == nullptr ? std::string{} : rider->name())})}};
                }
                const std::int32_t vehicleId = *riding->vehicleId;
                riding->vehicleId.reset();
                const Parsed<world::LoadedWorld*> world = context.world();
                std::string vehicleName = "?";
                if (world.has_value()) {
                    if (entity::ServerEntity* vehicle = (*world)->state().entity(vehicleId); vehicle != nullptr) {
                        std::erase(vehicle->passengerIds, rider->entityId());
                        vehicleName = vehicle->customName.value_or(std::string{vehicle->type().path()});
                    }
                }
                const play::SetEntityLinkPacket packet{.sourceId = rider->entityId(),
                                                       .destinationId = play::SetEntityLinkPacket::kNoVehicle};
                sendToWorld(context, play::SetEntityLinkPacket::kId,
                            [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
                context.success(Component::translatable("commands.ride.dismount.success",
                                                        {Component::text(rider->name()), Component::text(vehicleName)}),
                                true);
                return 1;
            });
        });
    });
}

RotateCommand::RotateCommand() : Command("rotate", permissionLevel::kGamemasters) {}

namespace {

/// parity: `Entity.lookAt`, which measures from the feet unless an anchor says otherwise, and whose
/// yaw is measured from +Z and grows clockwise.
[[nodiscard]] std::pair<float, float> anglesTowards(const CommandTarget& target, double x, double y, double z) {
    const double dx = x - target.x();
    const double dy = y - target.y();
    const double dz = z - target.z();
    return {static_cast<float>(-std::atan2(dx, dz) * kDegreesPerRadian),
            static_cast<float>(-std::atan2(dy, std::sqrt((dx * dx) + (dz * dz))) * kDegreesPerRadian)};
}

[[nodiscard]] Parsed<std::int32_t> rotate(const CommandContext& context, float yaw, float pitch) {
    const CommandTarget* target = only(context, "target");
    if (target == nullptr) {
        return 0;
    }
    if (player::PlayerSession* session = target->player(); session != nullptr) {
        if (session->connection().teleportTo) {
            session->connection().teleportTo(target->x(), target->y(), target->z(), yaw, pitch);
        }
    } else if (entity::ServerEntity* what = target->entity(); what != nullptr) {
        what->yaw = yaw;
        what->pitch = pitch;
    }
    // A dropped item is drawn spinning on its own clock, so it has no rotation to set.
    context.success(Component::translatable("commands.rotate.success", {Component::text(target->name())}), true);
    return 1;
}

} // namespace

void RotateCommand::build(const CommandNodeBuilder& root) const {
    root.argument("target", entitiesArgument(true), [](const CommandNodeBuilder& target) {
        target.argument("rotation", rotationArgument(), [](const CommandNodeBuilder& rotation) {
            rotation.executes([](const CommandContext& context) -> Parsed<std::int32_t> {
                const auto* facing = context.get<std::pair<float, float>>("rotation");
                return facing == nullptr ? Parsed<std::int32_t>{0} : rotate(context, facing->first, facing->second);
            });
        });
        target.literal("facing", [](const CommandNodeBuilder& facing) {
            facing.argument("facingLocation", vec3Argument(), [](const CommandNodeBuilder& at) {
                at.executes([](const CommandContext& context) -> Parsed<std::int32_t> {
                    const CommandTarget* target = only(context, "target");
                    const auto* location = context.get<Coordinates>("facingLocation");
                    if (target == nullptr || location == nullptr) {
                        return 0;
                    }
                    const std::array<double, 3> point = location->resolve(context.source().position());
                    const auto [yaw, pitch] = anglesTowards(*target, point[0], point[1], point[2]);
                    return rotate(context, yaw, pitch);
                });
            });
            facing.literal("entity", [](const CommandNodeBuilder& entity) {
                const auto face = [](const CommandContext& context, bool eyes) -> Parsed<std::int32_t> {
                    const CommandTarget* target = only(context, "target");
                    const CommandTarget* other = only(context, "facingEntity");
                    if (target == nullptr || other == nullptr) {
                        return 0;
                    }
                    const auto [yaw, pitch] =
                        anglesTowards(*target, other->x(), other->y() + (eyes ? kEyeHeight : 0.0), other->z());
                    return rotate(context, yaw, pitch);
                };
                entity.argument("facingEntity", entitiesArgument(true), [face](const CommandNodeBuilder& named) {
                    named.executes([face](const CommandContext& context) { return face(context, false); });
                    named.argument("facingAnchor", entityAnchorArgument(), [face](const CommandNodeBuilder& anchor) {
                        anchor.executes([face](const CommandContext& context) {
                            const auto* which = context.get<EntityAnchor>("facingAnchor");
                            return face(context, which != nullptr && *which == EntityAnchor::Eyes);
                        });
                    });
                });
            });
        });
    });
}

SwingCommand::SwingCommand() : Command("swing", permissionLevel::kGamemasters) {}

void SwingCommand::build(const CommandNodeBuilder& root) const {
    const auto swing = [](const CommandContext& context, bool offhand) -> Parsed<std::int32_t> {
        const std::vector<CommandTarget>& targets = targetsOf(context, "targets");
        const std::uint8_t action = offhand ? play::AnimatePacket::kSwingOffHand : play::AnimatePacket::kSwingMainHand;
        for (const CommandTarget& target : targets) {
            const play::AnimatePacket packet{.entityId = target.entityId(), .action = action};
            sendToWorld(context, play::AnimatePacket::kId,
                        [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        }
        context.success(
            targets.size() == 1
                ? Component::translatable("commands.swing.success.single", {Component::text(targets.front().name())})
                : Component::translatable("commands.swing.success.multiple",
                                          {Component::text(std::to_string(targets.size()))}),
            true);
        return static_cast<std::int32_t>(targets.size());
    };
    root.argument("targets", entitiesArgument(false), [swing](const CommandNodeBuilder& targets) {
        targets.executes([swing](const CommandContext& context) { return swing(context, false); });
        for (const bool offhand : {false, true}) {
            targets.literal(offhand ? "offhand" : "mainhand", [swing, offhand](const CommandNodeBuilder& hand) {
                hand.executes([swing, offhand](const CommandContext& context) { return swing(context, offhand); });
            });
        }
    });
}

} // namespace basalt::server::runtime::command::commands
