#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerInventory.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RealLootTables.hpp>
#include <basalt/server/runtime/world/TriggerFacts.hpp>

#include <array>
#include <string_view>
#include <utility>

namespace basalt::server::runtime::world::triggerFacts {
namespace {

namespace loot = server::world::loot;
using wire::common::Identifier;

/// parity: `DyeColor`, in the order a sheep's `Color` byte counts them.
constexpr std::array<std::string_view, 16> kDyeColors{"white", "orange", "magenta",    "light_blue", "yellow", "lime",
                                                      "pink",  "gray",   "light_gray", "cyan",       "purple", "blue",
                                                      "brown", "green",  "red",        "black"};

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

loot::LootContext ofPlayerAt(RealLootTables& tables, LoadedWorld& world, const player::PlayerSession& player) {
    loot::LootContext context{.hasEntity = true};
    tables.prepareAt(context, world, player.x, player.y, player.z);
    loot::EntityFacts facts = ofPlayer(player);
    facts.placeAt = context.placeAt;
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

} // namespace basalt::server::runtime::world::triggerFacts
