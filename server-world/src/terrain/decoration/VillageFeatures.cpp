#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/CactusFeature.hpp>
#include <basalt/server/world/terrain/decoration/PlacedFeatures.hpp>
#include <basalt/server/world/terrain/decoration/PlainsFlowerFeatures.hpp>
#include <basalt/server/world/terrain/decoration/SimpleBlockFeature.hpp>
#include <basalt/server/world/terrain/decoration/VillageFeatures.hpp>
#include <basalt/server/world/terrain/decoration/tree/TreeConfigs.hpp>
#include <basalt/server/world/terrain/decoration/tree/TreeFeature.hpp>

#include <array>
#include <cstdint>
#include <expected>
#include <functional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace basalt::server::world::terrain::decoration {
namespace {

using StateProvider = std::function<std::int32_t(random::RandomSource&)>;
using PathSet = std::set<std::string, std::less<>>;

[[nodiscard]] std::int32_t state(std::string_view path) {
    const registry::BlockState* found = registry::blocks::named(path);
    if (found == nullptr) {
        throw std::runtime_error{"unknown block '" + std::string{path} + "'"};
    }
    return found->stateId;
}

[[nodiscard]] std::string pathOf(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string{} : block->path();
}

const PathSet& supportsVegetationPaths() {
    static const PathSet paths = {"dirt",       "coarse_dirt",     "rooted_dirt", "grass_block",
                                  "podzol",     "mycelium",        "mud",         "muddy_mangrove_roots",
                                  "moss_block", "pale_moss_block", "farmland"};
    return paths;
}

/// parity: `RotatedBlockProvider.getState` over `Direction.Axis`, whose enum order is X, Y, Z.
const std::vector<std::int32_t>& hayAxisStates() {
    static const std::vector<std::int32_t> states = [] {
        std::vector<std::int32_t> built;
        for (const std::string_view axis : {"x", "y", "z"}) {
            const std::optional<std::int32_t> resolved = registry::blocks::stateIdOf(
                wire::common::Identifier::vanilla("hay_block"), {{"axis", std::string{axis}}});
            if (!resolved.has_value()) {
                throw std::runtime_error{"unknown hay_block axis"};
            }
            built.push_back(*resolved);
        }
        return built;
    }();
    return states;
}

[[nodiscard]] std::int32_t sweetBerryAge3StateId() {
    static const std::int32_t stateId = [] {
        const std::optional<std::int32_t> resolved =
            registry::blocks::stateIdOf(wire::common::Identifier::vanilla("sweet_berry_bush"), {{"age", "3"}});
        if (!resolved.has_value()) {
            throw std::runtime_error{"unknown mature sweet berry bush"};
        }
        return *resolved;
    }();
    return stateId;
}

/// parity: oak's placed-feature would-survive oak sapling predicate.
[[nodiscard]] bool supportsVegetation(const Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z) {
    const ChunkHeight bounds = chunk.verticalBounds();
    if (y < bounds.minY() || y > bounds.maxY()) {
        return false;
    }
    return supportsVegetationPaths().contains(pathOf(chunk.decorationBlockStateId(x, y, z)));
}

/// parity: `BlockPileFeature.tryPlaceBlock`.
void tryPlacePileBlock(Chunk& chunk, random::RandomSource& random, std::int32_t x, std::int32_t y, std::int32_t z,
                       std::int32_t dirtPathId, const StateProvider& stateProvider) {
    const ChunkHeight bounds = chunk.verticalBounds();
    if (y < bounds.minY() || y > bounds.maxY()) {
        return;
    }
    if (!registry::blocks::isAir(chunk.decorationBlockStateId(x, y, z)) || y - 1 < bounds.minY()) {
        return;
    }
    const std::int32_t below = chunk.decorationBlockStateId(x, y - 1, z);
    if (villageFeatures::mayPlacePileOn(below, dirtPathId, random)) {
        chunk.setDecorationBlockStateId(x, y, z, stateProvider(random));
    }
}

/// parity: `BlockPileFeature.place`.
bool placeBlockPile(Chunk& chunk, random::RandomSource& random, std::int32_t x, std::int32_t y, std::int32_t z,
                    const StateProvider& stateProvider) {
    if (y < chunk.verticalBounds().minY() + 5) {
        return false;
    }
    const std::int32_t xr = 2 + random.nextInt(2);
    const std::int32_t zr = 2 + random.nextInt(2);
    const std::int32_t dirtPathId = state("dirt_path");
    for (std::int32_t bz = z - zr; bz <= z + zr; ++bz) {
        for (std::int32_t by = y; by <= y + 1; ++by) {
            for (std::int32_t bx = x - xr; bx <= x + xr; ++bx) {
                const std::int32_t xd = x - bx;
                const std::int32_t zd = z - bz;
                const bool near =
                    static_cast<float>(xd * xd + zd * zd) <= random.nextFloat() * 10.0F - random.nextFloat() * 6.0F;
                if (near || random.nextFloat() < 0.031F) {
                    tryPlacePileBlock(chunk, random, bx, by, bz, dirtPathId, stateProvider);
                }
            }
        }
    }
    return true;
}

using PatchPredicate = std::function<bool(Chunk&, const DecorationPos&)>;
using PatchPlace = std::function<void(Chunk&, random::RandomSource&, const DecorationPos&)>;

/// The shape the three patch features share: `count` attempts at the piece position with a
/// trapezoid(7, 3) offset each, then the configured feature, which does its own target checks.
void placePatch(Chunk& chunk, random::RandomSource& random, std::int32_t x, std::int32_t y, std::int32_t z,
                std::int32_t count, const PatchPredicate& predicate, const PatchPlace& place) {
    const std::vector<PlacementModifier> modifiers = {
        countPlacement(count), randomOffsetPlacement(7, 3),
        filter([&predicate, &chunk](const PlacementContext&, random::RandomSource&, const DecorationPos& origin) {
            return predicate(chunk, origin);
        })};
    placeFrom(chunk, random, DecorationPos{x, y, z}, modifiers,
              [&](const DecorationPos& origin) { place(chunk, random, origin); });
}

[[nodiscard]] bool airTarget(Chunk& chunk, const DecorationPos& origin) {
    return registry::blocks::isAir(chunk.decorationBlockStateId(origin.x, origin.y, origin.z));
}

} // namespace

bool villageFeatures::mayPlacePileOn(const std::int32_t belowStateId, const std::int32_t dirtPathStateId,
                                     random::RandomSource& random) {
    if (belowStateId == dirtPathStateId) {
        return random.nextBoolean();
    }
    return registry::blockShapes::isFaceSturdy(belowStateId, registry::BlockFace::Up);
}

void villageFeatures::place(const std::string_view feature, Chunk& chunk, const std::int32_t x, const std::int32_t y,
                            const std::int32_t z, random::RandomSource& random) {
    if (feature == "minecraft:pile_hay") {
        static_cast<void>(placeBlockPile(chunk, random, x, y, z, [](random::RandomSource& r) {
            return hayAxisStates()[static_cast<std::size_t>(
                r.nextInt(static_cast<std::int32_t>(hayAxisStates().size())))];
        }));
        return;
    }
    if (feature == "minecraft:pile_melon") {
        static_cast<void>(placeBlockPile(chunk, random, x, y, z, [](random::RandomSource&) { return state("melon"); }));
        return;
    }
    if (feature == "minecraft:pile_snow") {
        static_cast<void>(placeBlockPile(chunk, random, x, y, z, [](random::RandomSource&) { return state("snow"); }));
        return;
    }
    if (feature == "minecraft:pile_ice") {
        // parity: pile_ice's weighted_state_provider, blue_ice weight 1 against packed_ice weight 5.
        static_cast<void>(placeBlockPile(chunk, random, x, y, z, [](random::RandomSource& r) {
            return r.nextInt(6) == 0 ? state("blue_ice") : state("packed_ice");
        }));
        return;
    }
    if (feature == "minecraft:pile_pumpkin") {
        // parity: pile_pumpkin's weighted_state_provider, pumpkin weight 19 against jack_o_lantern 1.
        static_cast<void>(placeBlockPile(chunk, random, x, y, z, [](random::RandomSource& r) {
            return r.nextInt(20) == 19 ? state("jack_o_lantern") : state("pumpkin");
        }));
        return;
    }
    if (feature == "minecraft:acacia" || feature == "minecraft:oak" || feature == "minecraft:spruce" ||
        feature == "minecraft:pine") {
        if (!supportsVegetation(chunk, x, y - 1, z)) {
            return;
        }
        const tree::TreeConfiguration& config = feature == "minecraft:acacia"   ? tree::treeConfigs::acacia()
                                                : feature == "minecraft:oak"    ? tree::treeConfigs::oak()
                                                : feature == "minecraft:spruce" ? tree::treeConfigs::spruce()
                                                                                : tree::treeConfigs::pine();
        static_cast<void>(tree::treeFeature::place(chunk, random, x, y, z, config));
        return;
    }
    if (feature == "minecraft:flower_plain") {
        plainsDecoration::placeFlowerPlainAt(chunk, random, x, y, z);
        return;
    }
    if (feature == "minecraft:patch_cactus") {
        placePatch(
            chunk, random, x, y, z, 10,
            [](Chunk& target, const DecorationPos& origin) { return cactusFeature::canPlace(target, origin); },
            [](Chunk& target, random::RandomSource& r, const DecorationPos& origin) {
                static_cast<void>(cactusFeature::place(target, r, origin));
            });
        return;
    }
    if (feature == "minecraft:patch_taiga_grass") {
        // parity: taiga_grass's weighted_state_provider, short_grass weight 1 against fern weight 4.
        placePatch(chunk, random, x, y, z, 32, airTarget,
                   [](Chunk& target, random::RandomSource& r, const DecorationPos& origin) {
                       const SimpleBlockFeature feature{[](random::RandomSource& providerRandom, const DecorationPos&) {
                           return providerRandom.nextInt(5) == 0 ? state("short_grass") : state("fern");
                       }};
                       static_cast<void>(feature.place(target, r, origin));
                   });
        return;
    }
    if (feature == "minecraft:patch_berry_bush") {
        placePatch(
            chunk, random, x, y, z, 96,
            [](Chunk& target, const DecorationPos& origin) {
                return registry::blocks::isAir(target.decorationBlockStateId(origin.x, origin.y, origin.z)) &&
                       pathOf(target.decorationBlockStateId(origin.x, origin.y - 1, origin.z)) == "grass_block";
            },
            [](Chunk& target, random::RandomSource& r, const DecorationPos& origin) {
                const SimpleBlockFeature feature{
                    [](random::RandomSource&, const DecorationPos&) { return sweetBerryAge3StateId(); }};
                static_cast<void>(feature.place(target, r, origin));
            });
        return;
    }
    // parity: any other `feature_pool_element`, such as the 26.3 camps' trees, is its placed
    // feature run from the piece's own position.
    if (const std::expected<wire::common::Identifier, wire::common::IdentifierError> id =
            wire::common::Identifier::parse(feature);
        id.has_value()) {
        if (const placedFeatures::Placement* placement = placedFeatures::find(*id); placement != nullptr) {
            (*placement)(chunk, random, x, y, z);
        }
    }
}

} // namespace basalt::server::world::terrain::decoration
