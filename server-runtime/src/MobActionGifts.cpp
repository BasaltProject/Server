#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/world/DroppedItem.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RandomSequences.hpp>
#include <basalt/server/runtime/world/RealLootTables.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <cmath>
#include <numbers>
#include <string>

namespace basalt::server::runtime {

void MobActionSystem::dropGift(entity::ServerEntity& mob, std::string_view table) {
    world::LoadedWorld* world = worldOf(mob);
    const std::expected<wire::common::Identifier, wire::common::IdentifierError> id =
        wire::common::Identifier::parse(std::string{table});
    if (world == nullptr || !id.has_value()) {
        return;
    }
    const server::world::loot::LootTable* loot = lootTables_->byId(*id);
    if (loot == nullptr) {
        return;
    }
    // parity: `dropFromGiftLootTable`, the `gift` set: the mob itself and where it stands.
    server::world::loot::LootContext context = deathLootContext(*world, mob);
    context.killedByPlayer = false;
    random::RandomSource scratch = freshRandom();
    random::RandomSource& random =
        loot->randomSequence.has_value() ? world->randomSequences().of(*loot->randomSequence) : scratch;
    // parity: the stack lands one step in front of where the body faces, from the block it is in.
    const auto radians = static_cast<double>(mob.yaw * (std::numbers::pi_v<float> / 180.0F));
    const double x = std::floor(mob.x) - static_cast<double>(wire::common::mth::sin(radians));
    const double y = std::floor(mob.y);
    const double z = std::floor(mob.z) + static_cast<double>(wire::common::mth::cos(radians));
    for (const server::world::loot::ItemStack& stack : loot->generate(random, context.luck, std::nullopt, context)) {
        if (server::registry::items::registry().byName(stack.item) == nullptr) {
            continue;
        }
        world->state().spawnItem(world::DroppedItem{.entityId = players_->allocateEntityId(),
                                                    .uuid = wire::common::Uuid::random(),
                                                    .item = stack.item,
                                                    .count = stack.count,
                                                    .x = x,
                                                    .y = y,
                                                    .z = z,
                                                    .components = stack.components});
    }
}

} // namespace basalt::server::runtime
