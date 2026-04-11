#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/entity/BlockRay.hpp>
#include <basalt/server/runtime/entity/MobBreathing.hpp>
#include <basalt/server/runtime/entity/Projectiles.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RealLootTables.hpp>
#include <basalt/server/runtime/world/TriggerFacts.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <array>
#include <cmath>
#include <limits>
#include <numbers>
#include <string_view>
#include <utility>

namespace basalt::server::runtime::world::triggerFacts {
namespace {

namespace loot = server::world::loot;
namespace mth = wire::common::mth;
using wire::common::Identifier;

/// parity: `DyeColor`, in the order a sheep's `Color` byte counts them.
constexpr std::array<std::string_view, 16> kDyeColors{"white", "orange", "magenta",    "light_blue", "yellow", "lime",
                                                      "pink",  "gray",   "light_gray", "cyan",       "purple", "blue",
                                                      "brown", "green",  "red",        "black"};

/// parity: `LOOKING_AT_RANGE` and `hasLineOfSight`'s reach.
constexpr double kLookRange = 100.0;
constexpr double kSightRange = 128.0;
constexpr float kRadians = static_cast<float>(std::numbers::pi / 180.0);

/// parity: the player's standing and crouching boxes and eyes.
constexpr double kPlayerWidth = 0.6;
constexpr double kStandingHeight = 1.8;
constexpr double kSneakingHeight = 1.5;
constexpr double kStandingEye = 1.62;
constexpr double kSneakingEye = 1.27;

/// The worn pieces in the inventory's own order, head first.
constexpr std::array<std::string_view, 4> kArmourSlots{"head", "chest", "legs", "feet"};

constexpr std::array<std::string_view, 7> kEquipmentSlots{"mainhand", "offhand", "head", "chest",
                                                          "legs",     "feet",    "body"};

void wear(loot::EntityFacts& facts, std::string_view slot, const std::optional<storage::StoredItemStack>& stack) {
    if (!stack.has_value() || stack->count <= 0) {
        return;
    }
    const std::optional<TriggerItem> item = itemOf(stack);
    if (item.has_value()) {
        facts.equipment.emplace(
            std::string{slot},
            loot::EquippedItem{.item = item->item, .count = item->count, .components = item->components});
    }
}

template <typename Effects>
void addEffects(loot::EntityFacts& facts, const Effects& effects) {
    for (const auto& [id, active] : effects) {
        facts.effects.emplace(id.toString(), loot::EffectFacts{.amplifier = active.amplifier,
                                                               .duration = active.remainingTicks,
                                                               .ambient = active.ambient,
                                                               .visible = active.visible});
    }
}

void addHeld(loot::EntityFacts& facts) {
    const auto held = facts.equipment.find("mainhand");
    if (held == facts.equipment.end()) {
        return;
    }
    facts.mainhand = held->second.item;
    const storage::StoredItemStack stack{
        .id = held->second.item.toString(), .count = held->second.count, .components = held->second.components};
    for (const auto& [enchantment, level] : player::enchantmentLevels(stack)) {
        facts.mainhandEnchantments.emplace(enchantment.toString(), level);
    }
}

} // namespace

Identifier damageTypeOf(std::string_view cause) {
    if (cause == "attack" || cause == "sweep") {
        return Identifier::vanilla("player_attack");
    }
    if (cause == "projectile") {
        return Identifier::vanilla("arrow");
    }
    return Identifier::vanilla(std::string{cause});
}

std::optional<TriggerItem> itemOf(const std::optional<storage::StoredItemStack>& stack) {
    if (!stack.has_value()) {
        return std::nullopt;
    }
    const std::expected<Identifier, wire::common::IdentifierError> id = Identifier::parse(stack->id);
    if (!id.has_value()) {
        return std::nullopt;
    }
    return TriggerItem{.item = *id, .count = stack->count, .components = stack->components};
}

loot::EntityFacts ofEntity(const LoadedWorld& world, const entity::ServerEntity& entity) {
    loot::EntityFacts facts = ofEntity(entity);
    if (entity.vehicleId.has_value()) {
        if (const entity::ServerEntity* vehicle = world.state().entity(*entity.vehicleId); vehicle != nullptr) {
            facts.vehicle = vehicle->type();
        }
    }
    return facts;
}

loot::EntityFacts ofEntity(const entity::ServerEntity& entity) {
    loot::EntityFacts facts{.type = entity.type(),
                            .onFire = entity.fireTicks > 0,
                            .baby = entity.age < 0,
                            .cubeSize = entity.cubeSize,
                            .captain = entity.patrolLeader,
                            .scoreName = entity.uuid().toString()};
    facts.position = std::array<double, 3>{entity.x, entity.y, entity.z};
    const std::string_view path = entity.type().path();
    if (entity.sourceNbt.has_value()) {
        const wire::nbt::NbtCompound& data = *entity.sourceNbt;
        if (path == "sheep") {
            facts.components.emplace("minecraft:sheep/color",
                                     std::string{kDyeColors[static_cast<std::size_t>(data.getByte("Color") & 0x0F)]});
            facts.sheared = data.getBoolean("Sheared");
        } else if (path == "mooshroom") {
            facts.components.emplace("minecraft:mooshroom/variant",
                                     data.getString("Type").empty() ? "red" : std::string{data.getString("Type")});
        }
        facts.baby = facts.baby || data.getBoolean("IsBaby");
        if (const wire::nbt::NbtCompound* equipment = data.getCompound("equipment"); equipment != nullptr) {
            for (const std::string_view slot : kEquipmentSlots) {
                const wire::nbt::NbtCompound* worn = equipment->getCompound(slot);
                if (worn == nullptr) {
                    continue;
                }
                const wire::nbt::NbtCompound* components = worn->getCompound("components");
                wear(facts, slot,
                     storage::StoredItemStack{.id = std::string{worn->getString("id")},
                                              .count = worn->getInt("count", 1),
                                              .components =
                                                  components == nullptr ? wire::nbt::NbtCompound{} : *components});
            }
        }
    } else if (path == "sheep") {
        facts.components.emplace("minecraft:sheep/color", "white");
    }
    if (entity.heldItem.has_value() && !facts.equipment.contains("mainhand")) {
        wear(facts, "mainhand",
             storage::StoredItemStack{.id = *entity.heldItem, .count = 1, .components = entity.heldItemComponents});
    }
    addHeld(facts);
    facts.flags.emplace("is_on_ground", entity.onGround);
    addEffects(facts, entity.effects);
    return facts;
}

loot::EntityFacts ofPlayer(const player::PlayerSession& player) {
    loot::EntityFacts facts{
        .type = Identifier::vanilla("player"), .onFire = player.fireTicks > 0, .scoreName = std::string{player.name()}};
    facts.position = std::array<double, 3>{player.x, player.y, player.z};
    const player::PlayerInventory& inventory = player.inventory;
    wear(facts, "mainhand", inventory.heldItem());
    wear(facts, "offhand", inventory.at(player::PlayerInventory::kOffhandSlot));
    for (std::int32_t index = 0; index < player::PlayerInventory::kArmourSlots; ++index) {
        wear(facts, kArmourSlots[static_cast<std::size_t>(index)],
             inventory.at(player::PlayerInventory::kArmourStart + index));
    }
    addHeld(facts);
    facts.flags = {{"is_on_ground", player.onGround},
                   {"is_sneaking", player.sneaking},
                   {"is_sprinting", player.sprinting},
                   {"is_flying", player.fallFlying || player.flying},
                   {"is_fall_flying", player.fallFlying}};
    addEffects(facts, player.effects);
    return facts;
}

loot::LootContext ofPlayerAt(RealLootTables& tables, LoadedWorld& world, const player::PlayerSession& player,
                             const player::PlayerRegistry* players) {
    loot::LootContext context{.hasEntity = true};
    tables.prepareAt(context, world, player.x, player.y, player.z);
    loot::EntityFacts facts = ofPlayer(player);
    facts.placeAt = context.placeAt;
    facts.findLookingAt = [&world, &player, players] { return lookingAtOf(world, player, players); };
    if (player.vehicleId.has_value()) {
        if (const entity::ServerEntity* vehicle = world.state().entity(*player.vehicleId); vehicle != nullptr) {
            facts.vehicle = vehicle->type();
            facts.vehicleFacts = std::make_shared<const loot::EntityFacts>(ofEntity(world, *vehicle));
        }
    }
    context.thisEntity = std::move(facts);
    return context;
}

loot::LootContext atBlock(RealLootTables& tables, LoadedWorld& world, const player::PlayerSession& player,
                          std::int32_t x, std::int32_t y, std::int32_t z,
                          const std::optional<storage::StoredItemStack>& tool, std::optional<std::int32_t> stateId) {
    loot::LootContext context{.hasEntity = true};
    tables.prepareAt(context, world, x + 0.5, y + 0.5, z + 0.5);
    if (!stateId.has_value()) {
        stateId = world.state().blockState(x, y, z);
    }
    if (stateId.has_value()) {
        if (const Identifier* block = server::registry::blockStateDefinitions::blockOf(*stateId); block != nullptr) {
            context.block = *block;
        }
        context.blockState = server::registry::blockStateDefinitions::propertiesOf(*stateId);
        // The square itself as it was, the rest of the world as it is.
        context.placeAt = [inner = context.placeAt, block = context.block, state = context.blockState, x, y,
                           z](std::int32_t bx, std::int32_t by, std::int32_t bz) {
            loot::PlaceFacts place = inner(bx, by, bz);
            if (bx == x && by == y && bz == z) {
                place.block = block;
                place.properties = state;
            }
            return place;
        };
    }
    loot::EntityFacts facts = ofPlayer(player);
    facts.placeAt = context.placeAt;
    context.thisEntity = std::move(facts);
    if (const std::optional<TriggerItem> held = itemOf(tool); held.has_value()) {
        context.tool = held->item;
        context.toolComponents = held->components;
        for (const auto& [enchantment, level] : player::enchantmentLevels(*tool)) {
            context.toolEnchantments.emplace(enchantment.toString(), level);
        }
    }
    return context;
}

vibrations::Source vibrationSourceOf(const player::PlayerSession& player, AdvancementTriggers* triggers) {
    vibrations::Source source{.entity = player.uuid(), .steppingCarefully = player.sneaking};
    if (triggers != nullptr && player.sneaking) {
        source.avoided = [triggers, name = std::string{player.name()}, uuid = player.uuid().toString()] {
            triggers->fire("avoid_vibration", name, uuid);
        };
    }
    return source;
}

std::optional<loot::EntityFacts> ofId(const LoadedWorld& world, const std::vector<player::PlayerSession*>& players,
                                      std::int32_t id) {
    for (const player::PlayerSession* player : players) {
        if (player != nullptr && player->entityId() == id) {
            return ofPlayer(*player);
        }
    }
    const entity::ServerEntity* mob = world.state().entity(id);
    return mob == nullptr ? std::nullopt : std::optional{ofEntity(world, *mob)};
}

void playerHurtEntity(AdvancementTriggers& triggers, const LoadedWorld& world, const player::PlayerSession& player,
                      const entity::ServerEntity& victim, float dealt, const Identifier& damageType,
                      const std::optional<loot::EntityFacts>& direct) {
    if (!triggers.listensTo("player_hurt_entity")) {
        return;
    }
    const loot::EntityFacts attacker = ofPlayer(player);
    triggers.fire("player_hurt_entity", player.name(), player.uuid().toString(),
                  TriggerEvent{.entities = {{"entity", ofEntity(world, victim)}},
                               .damage = TriggerDamage{.dealt = dealt,
                                                       .taken = dealt,
                                                       .type = damageType,
                                                       .direct = direct.has_value() ? direct : attacker,
                                                       .source = attacker,
                                                       .directHit = !direct.has_value()}});
}

std::shared_ptr<const loot::EntityFacts> lookingAtOf(LoadedWorld& world, const player::PlayerSession& player,
                                                     const player::PlayerRegistry* players) {
    const double eyeY = player.y + (player.sneaking ? kSneakingEye : kStandingEye);
    // parity: `Entity.calculateViewVector`, through the sine table.
    const float pitch = player.pitch * kRadians;
    const float yaw = -player.yaw * kRadians;
    const auto cosYaw = static_cast<double>(mth::cos(yaw));
    const auto sinYaw = static_cast<double>(mth::sin(yaw));
    const auto cosPitch = static_cast<double>(mth::cos(pitch));
    const auto sinPitch = static_cast<double>(mth::sin(pitch));
    const std::array<double, 3> from{player.x, eyeY, player.z};
    const std::array<double, 3> to{from[0] + (sinYaw * cosPitch * kLookRange), from[1] - (sinPitch * kLookRange),
                                   from[2] + (cosYaw * cosPitch * kLookRange)};
    double nearest = std::numeric_limits<double>::max();
    std::optional<loot::EntityFacts> found;
    std::array<double, 3> foundEye{};
    const auto consider = [&](double x, double y, double z, double width, double height, double eye,
                              const auto& facts) {
        const std::optional<double> at =
            entity::projectiles::clipBox(from[0], from[1], from[2], to[0], to[1], to[2], x - (width / 2.0), y,
                                         z - (width / 2.0), x + (width / 2.0), y + height, z + (width / 2.0));
        if (at.has_value() && *at < nearest) {
            nearest = *at;
            found = facts();
            foundEye = {x, y + eye, z};
        }
    };
    for (const entity::ServerEntity* other : world.state().entities()) {
        if (other->removed) {
            continue;
        }
        consider(other->x, other->y, other->z, other->width(), other->height(),
                 entity::mobBreathing::eyeHeightOf(*other), [&world, other] { return ofEntity(world, *other); });
    }
    if (players != nullptr) {
        for (const player::PlayerSession* other : players->snapshot()) {
            // parity: the pick leaves out the player looking and anyone in spectator.
            if (other == &player || other->worldName != player.worldName ||
                other->gamemode == wire::protocol::play::Gamemode::Spectator) {
                continue;
            }
            consider(other->x, other->y, other->z, kPlayerWidth, other->sneaking ? kSneakingHeight : kStandingHeight,
                     other->sneaking ? kSneakingEye : kStandingEye, [other] { return ofPlayer(*other); });
        }
    }
    if (!found.has_value()) {
        return nullptr;
    }
    // parity: `LivingEntity.hasLineOfSight`, eye to eye through what collides, and never past 128.
    const double dx = foundEye[0] - from[0];
    const double dy = foundEye[1] - from[1];
    const double dz = foundEye[2] - from[2];
    if (std::sqrt((dx * dx) + (dy * dy) + (dz * dz)) > kSightRange ||
        entity::blockRay::cast(world.state(), from[0], from[1], from[2], foundEye[0], foundEye[1], foundEye[2])
            .has_value()) {
        return nullptr;
    }
    return std::make_shared<const loot::EntityFacts>(*std::move(found));
}

} // namespace basalt::server::runtime::world::triggerFacts
