#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/player/PlayerSounds.hpp>
#include <basalt/server/runtime/world/AttachedEntities.hpp>
#include <basalt/server/runtime/world/DroppedItem.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/ItemFrames.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/Paintings.hpp>

#include <cmath>
#include <limits>

namespace basalt::server::runtime::world::attachedEntities {
namespace {

[[nodiscard]] bool isPainting(const wire::common::Identifier& type) { return type.path() == "painting"; }

/// parity: `Cushion.destroyIfInFire` over the box less an ulp, and lava's `entityInside`, which
/// sets it alight and hurts it off its block.
[[nodiscard]] bool burning(const WorldState& world, const entity::ServerEntity& cushion,
                           const cushions::Tagged& tagged) {
    const double half = cushions::kWidth / 2.0;
    const auto inward = [](double value, double towards) { return std::nextafter(value, towards); };
    const double infinity = std::numeric_limits<double>::infinity();
    const auto lowX = static_cast<std::int32_t>(std::floor(inward(cushion.x - half, infinity)));
    const auto lowY = static_cast<std::int32_t>(std::floor(inward(cushion.y, infinity)));
    const auto lowZ = static_cast<std::int32_t>(std::floor(inward(cushion.z - half, infinity)));
    const auto highX = static_cast<std::int32_t>(std::floor(inward(cushion.x + half, -infinity)));
    const auto highY = static_cast<std::int32_t>(std::floor(inward(cushion.y + cushions::kHeight, -infinity)));
    const auto highZ = static_cast<std::int32_t>(std::floor(inward(cushion.z + half, -infinity)));
    for (std::int32_t x = lowX; x <= highX; ++x) {
        for (std::int32_t y = lowY; y <= highY; ++y) {
            for (std::int32_t z = lowZ; z <= highZ; ++z) {
                const std::optional<std::int32_t> state = world.blockState(x, y, z);
                const wire::common::Identifier* block =
                    state.has_value() ? server::registry::blockStateDefinitions::blockOf(*state) : nullptr;
                if (block != nullptr && tagged && tagged("minecraft:fire", *block)) {
                    return true;
                }
            }
        }
    }
    const std::optional<std::int32_t> at = world.blockState(static_cast<std::int32_t>(std::floor(cushion.x)),
                                                            static_cast<std::int32_t>(std::floor(cushion.y)),
                                                            static_cast<std::int32_t>(std::floor(cushion.z)));
    const wire::common::Identifier* block =
        at.has_value() ? server::registry::blockStateDefinitions::blockOf(*at) : nullptr;
    return block != nullptr && block->path() == "lava";
}

/// The item it drops, carrying the name it was given. parity: `get*ItemStackWithData`.
[[nodiscard]] server::world::loot::ItemStack withName(const entity::ServerEntity& attached,
                                                      wire::common::Identifier item) {
    server::world::loot::ItemStack stack{.item = std::move(item), .count = 1};
    if (attached.sourceNbt.has_value()) {
        if (const wire::nbt::NbtTag* name = attached.sourceNbt->find("CustomName"); name != nullptr) {
            stack.components.set("minecraft:custom_name", *name);
        }
    }
    return stack;
}

} // namespace

bool isAttached(const wire::common::Identifier& type) {
    return itemFrames::isFrame(type) || isPainting(type) || cushions::isCushion(type);
}

bool dueForCheck(entity::ServerEntity& attached) {
    if (attached.attachedCheckTicks++ < kCheckInterval) {
        return false;
    }
    attached.attachedCheckTicks = 0;
    return true;
}

bool survives(const WorldState& world, const entity::ServerEntity& attached, const cushions::Tagged& tagged) {
    if (itemFrames::isFrame(attached.type())) {
        return itemFrames::survives(world, attached);
    }
    if (isPainting(attached.type())) {
        return paintings::survives(world, attached);
    }
    if (cushions::isCushion(attached.type())) {
        return !burning(world, attached, tagged) &&
               cushions::survivesAt(world, attached.x, attached.y, attached.z, tagged);
    }
    return true;
}

std::vector<server::world::loot::ItemStack> drops(const entity::ServerEntity& attached, random::RandomSource& random) {
    std::vector<server::world::loot::ItemStack> out;
    if (cushions::isCushion(attached.type())) {
        out.push_back(withName(attached, cushions::itemOf(attached)));
    } else if (isPainting(attached.type())) {
        out.push_back(withName(attached, wire::common::Identifier::vanilla("painting")));
    } else if (itemFrames::isFrame(attached.type())) {
        // parity: `ItemFrame.dropItem(level, null, true)`: nothing at all from a fixed frame, else
        // the frame and then what it held, against its drop chance.
        if (attached.sourceNbt.has_value() && attached.sourceNbt->getBoolean("Fixed")) {
            return out;
        }
        out.push_back(withName(attached, attached.type()));
        if (const std::optional<server::world::loot::ItemStack> held = itemFrames::shownIn(attached);
            held.has_value()) {
            const float chance =
                attached.sourceNbt.has_value() ? attached.sourceNbt->getFloat("ItemDropChance", 1.0F) : 1.0F;
            if (random.nextFloat() < chance) {
                out.push_back(*held);
            }
        }
    }
    return out;
}

std::string_view breakSound(const wire::common::Identifier& type) {
    if (cushions::isCushion(type)) {
        return "minecraft:entity.cushion.break";
    }
    if (isPainting(type)) {
        return "minecraft:entity.painting.break";
    }
    return type.path() == "glow_item_frame" ? "minecraft:entity.glow_item_frame.break"
                                            : "minecraft:entity.item_frame.break";
}

void comeOff(LoadedWorld& world, entity::ServerEntity& attached, const std::vector<player::PlayerSession*>& watchers,
             random::RandomSource& random, const std::function<std::int32_t()>& allocateEntityId) {
    attached.removed = true;
    const bool dropping = world.gameRules().get(gameRules::kEntityDrops);
    // parity: a painting's `dropItem` is silent with drops off; a frame's and a cushion's are not.
    if (dropping || attached.type().path() != "painting") {
        for (const player::PlayerSession* listener : watchers) {
            player::playSoundFor(*listener, breakSound(attached.type()), wire::protocol::play::SoundSource::Neutral,
                                 attached.x, attached.y, attached.z, 1.0F, 1.0F, random.nextLong());
        }
    }
    if (!dropping || !allocateEntityId) {
        return;
    }
    // parity: `Entity.spawnAtLocation`, an `ItemEntity`'s own scatter from where it hung.
    for (const server::world::loot::ItemStack& stack : drops(attached, random)) {
        static_cast<void>(world.state().spawnItem(DroppedItem{.entityId = allocateEntityId(),
                                                              .uuid = wire::common::Uuid::random(),
                                                              .item = stack.item,
                                                              .count = stack.count,
                                                              .x = attached.x,
                                                              .y = attached.y,
                                                              .z = attached.z,
                                                              .velocityX = (random.nextDouble() * 0.2) - 0.1,
                                                              .velocityY = 0.2,
                                                              .velocityZ = (random.nextDouble() * 0.2) - 0.1,
                                                              .components = stack.components}));
    }
}

} // namespace basalt::server::runtime::world::attachedEntities
