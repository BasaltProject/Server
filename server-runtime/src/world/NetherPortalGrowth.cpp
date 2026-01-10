#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/world/NetherPortalGrowth.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/common/Uuid.hpp>

#include <cstdint>
#include <memory>
#include <optional>

namespace basalt::server::runtime::world::netherPortalGrowth {
namespace {

/// parity: `NetherPortalBlock.randomTick`'s `nextInt(2000) < difficulty`.
constexpr std::int32_t kPortalOdds = 2000;

[[nodiscard]] const wire::common::Identifier& zombifiedPiglin() {
    static const wire::common::Identifier id = wire::common::Identifier::vanilla("zombified_piglin");
    return id;
}

[[nodiscard]] std::int32_t difficultyOf(std::string_view difficulty) {
    if (difficulty == "peaceful") {
        return 0;
    }
    if (difficulty == "easy") {
        return 1;
    }
    return difficulty == "normal" ? 2 : 3;
}

[[nodiscard]] bool isPortal(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> state = world.blockState(x, y, z);
    if (!state.has_value()) {
        return false;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*state);
    return block != nullptr && block->path() == "nether_portal";
}

} // namespace

entity::ServerEntity*
netherPortal(WorldState& world, const GameRuleValues& gameRules, std::string_view difficulty, bool spawnsPortalPiglins,
             std::int32_t x, std::int32_t y, std::int32_t z, random::RandomSource& random,
             const std::function<std::int32_t()>& allocateEntityId,
             const std::function<bool(std::int32_t, std::int32_t, std::int32_t)>& anyPlayerCloseEnough) {
    if (!spawnsPortalPiglins) {
        return nullptr;
    }
    if (!gameRules.get(gameRules::kSpawnMonsters)) {
        return nullptr;
    }
    const std::int32_t difficultyId = difficultyOf(difficulty);
    if (difficultyId <= 0) {
        return nullptr;
    }
    if (random.nextInt(kPortalOdds) >= difficultyId) {
        return nullptr;
    }
    // parity: vanilla asks for a player last, after the roll, so the sequence is the same either way.
    if (anyPlayerCloseEnough && !anyPlayerCloseEnough(x, y, z)) {
        return nullptr;
    }

    // parity: the walk down to the floor of the portal, which is where the piglin stands on.
    std::int32_t floorY = y;
    while (isPortal(world, x, floorY, z)) {
        --floorY;
    }
    const std::optional<std::int32_t> ground = world.blockState(x, floorY, z);
    if (!ground.has_value()) {
        return nullptr;
    }
    if (!registry::blockShapes::isFaceSturdy(*ground, registry::BlockFace::Up)) {
        return nullptr;
    }
    const registry::EntityTypeEntry* type = registry::entityTypes::of(zombifiedPiglin());
    if (type == nullptr) {
        return nullptr;
    }

    auto entity =
        std::make_unique<entity::ServerEntity>(allocateEntityId(), wire::common::Uuid::random(), zombifiedPiglin(),
                                               type->protocolId, x + 0.5, static_cast<double>(floorY + 1), z + 0.5);
    return &world.spawnEntity(std::move(entity));
}

} // namespace basalt::server::runtime::world::netherPortalGrowth
