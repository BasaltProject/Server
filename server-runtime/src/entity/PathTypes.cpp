#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/BlockSupport.hpp>
#include <basalt/server/runtime/entity/PathTypes.hpp>

#include <map>
#include <string>
#include <string_view>

namespace basalt::server::runtime::entity::pathTypes {
namespace {

using namespace std::string_view_literals;

[[nodiscard]] bool endsWith(std::string_view path, std::string_view suffix) { return path.ends_with(suffix); }

[[nodiscard]] std::string property(std::int32_t stateId, std::string_view name) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find(std::string{name});
    return found == properties.end() ? std::string{} : found->second;
}

[[nodiscard]] bool isWall(std::string_view path) { return endsWith(path, "_wall"); }

[[nodiscard]] bool isFence(std::string_view path) { return endsWith(path, "_fence"); }

/// parity: `BlockBehaviour.isPathfindable(LAND)` and the blocks that answer it their own way.
[[nodiscard]] bool pathfindableOnLand(std::int32_t stateId, std::string_view path) {
    static constexpr std::array kNever{"anvil"sv,
                                       "chipped_anvil"sv,
                                       "damaged_anvil"sv,
                                       "azalea"sv,
                                       "flowering_azalea"sv,
                                       "bamboo"sv,
                                       "bell"sv,
                                       "campfire"sv,
                                       "soul_campfire"sv,
                                       "brewing_stand"sv,
                                       "chest"sv,
                                       "trapped_chest"sv,
                                       "cake"sv,
                                       "cactus"sv,
                                       "composter"sv,
                                       "chorus_plant"sv,
                                       "end_portal_frame"sv,
                                       "iron_bars"sv,
                                       "glass_pane"sv,
                                       "dragon_egg"sv,
                                       "dried_ghast"sv,
                                       "cocoa"sv,
                                       "farmland"sv,
                                       "conduit"sv,
                                       "flower_pot"sv,
                                       "hopper"sv,
                                       "enchanting_table"sv,
                                       "decorated_pot"sv,
                                       "ender_chest"sv,
                                       "grindstone"sv,
                                       "dirt_path"sv,
                                       "mud"sv,
                                       "heavy_core"sv,
                                       "sculk_sensor"sv,
                                       "calibrated_sculk_sensor"sv,
                                       "lectern"sv,
                                       "lantern"sv,
                                       "soul_lantern"sv,
                                       "sniffer_egg"sv,
                                       "respawn_anchor"sv,
                                       "sea_pickle"sv,
                                       "pointed_dripstone"sv,
                                       "sulfur_spike"sv,
                                       "end_rod"sv,
                                       "shelf_mushroom"sv,
                                       "stonecutter"sv,
                                       "soul_sand"sv,
                                       "cauldron"sv,
                                       "water_cauldron"sv,
                                       "lava_cauldron"sv,
                                       "powder_snow_cauldron"sv,
                                       "iron_chain"sv};
    for (const std::string_view never : kNever) {
        if (path == never) {
            return false;
        }
    }
    if (endsWith(path, "_bed") || endsWith(path, "_skull") || (endsWith(path, "_head") && path != "piston_head") ||
        endsWith(path, "_chest") || endsWith(path, "candle_cake") || endsWith(path, "_bars") ||
        endsWith(path, "_pane") || endsWith(path, "copper_golem_statue") || path.starts_with("potted_") ||
        endsWith(path, "_lantern") || endsWith(path, "_shelf") || endsWith(path, "lightning_rod") ||
        endsWith(path, "_chain") || endsWith(path, "_stairs") || endsWith(path, "_slab") ||
        endsWith(path, "_wall_hanging_sign") || isFence(path) || isWall(path)) {
        return false;
    }
    if (endsWith(path, "_door") || endsWith(path, "_trapdoor") || endsWith(path, "_fence_gate")) {
        return property(stateId, "open") == "true";
    }
    if (path == "water" || path == "powder_snow") {
        return true;
    }
    if (path == "lava") {
        return false;
    }
    if (path == "snow") {
        const std::string layers = property(stateId, "layers");
        return layers.empty() || layers[0] < '5';
    }
    return !registry::blockShapes::isFullCube(stateId);
}

[[nodiscard]] PathType classify(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr) {
        return PathType::Blocked;
    }
    const std::string_view path = block->path();
    if (path == "air" || path == "cave_air" || path == "void_air") {
        return PathType::Open;
    }
    if (endsWith(path, "_trapdoor") || path == "lily_pad" || path == "big_dripleaf") {
        return PathType::Trapdoor;
    }
    if (path == "powder_snow") {
        return PathType::PowderSnow;
    }
    if (path == "cactus" || path == "sweet_berry_bush") {
        return PathType::Damaging;
    }
    if (path == "honey_block") {
        return PathType::StickyHoney;
    }
    if (path == "cocoa") {
        return PathType::Cocoa;
    }
    // parity: `BlockTags.SPELEOTHEMS`, the pointed dripstone and the sulfur spike.
    if (path == "wither_rose" || path == "pointed_dripstone" || path == "sulfur_spike") {
        return PathType::DamageCautious;
    }
    if (path == "lava") {
        return PathType::Lava;
    }
    // parity: `NodeEvaluator.isBurningBlock`.
    if (path == "fire" || path == "soul_fire" || path == "magma_block" || path == "lava_cauldron" ||
        ((path == "campfire" || path == "soul_campfire") && property(stateId, "lit") == "true")) {
        return PathType::Fire;
    }
    if (endsWith(path, "_door")) {
        if (property(stateId, "open") == "true") {
            return PathType::DoorOpen;
        }
        return path == "iron_door" ? PathType::DoorIronClosed : PathType::DoorWoodClosed;
    }
    if (path == "rail" || endsWith(path, "_rail")) {
        return PathType::Rail;
    }
    if (endsWith(path, "_leaves")) {
        return PathType::Leaves;
    }
    if (isFence(path) || isWall(path) || (endsWith(path, "_fence_gate") && property(stateId, "open") != "true")) {
        return PathType::Fence;
    }
    if (!pathfindableOnLand(stateId, path)) {
        return PathType::Blocked;
    }
    return registry::blockSupport::holdsWater(stateId) ? PathType::Water : PathType::Open;
}

} // namespace

bool isPathfindableOnLand(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block != nullptr && pathfindableOnLand(stateId, block->path());
}

const Maluses& defaults() {
    static const Maluses kDefaults{-1.0F, 0.0F,  0.0F, 0.0F,  0.0F, -1.0F, 0.0F, -1.0F, -1.0F,
                                   8.0F,  8.0F,  0.0F, -1.0F, 8.0F, 16.0F, 8.0F, -1.0F, 0.0F,
                                   -1.0F, -1.0F, 4.0F, -1.0F, 8.0F, 0.0F,  0.0F, 0.0F,  4.0F};
    return kDefaults;
}

Maluses of(const wire::common::Identifier& type) {
    Maluses maluses = defaults();
    const auto set = [&maluses](PathType pathType, float cost) { maluses[static_cast<std::size_t>(pathType)] = cost; };
    const std::string_view path = type.path();
    if (path == "parrot") {
        set(PathType::FireInNeighbor, -1.0F);
        set(PathType::Fire, -1.0F);
        set(PathType::Cocoa, -1.0F);
    } else if (path == "bee") {
        set(PathType::Fire, -1.0F);
        set(PathType::Water, -1.0F);
        set(PathType::WaterBorder, 16.0F);
        set(PathType::Cocoa, -1.0F);
        set(PathType::Fence, -1.0F);
    }
    return maluses;
}

PathType fromState(const world::WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> state = world.blockState(x, y, z);
    // parity: a square not loaded reads as air in `PathNavigationRegion`, which is open.
    return state.has_value() ? classify(*state) : PathType::Open;
}

PathType checkNeighbours(const world::WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                         PathType type) {
    for (std::int32_t dx = -1; dx <= 1; ++dx) {
        for (std::int32_t dy = -1; dy <= 1; ++dy) {
            for (std::int32_t dz = -1; dz <= 1; ++dz) {
                if (dx == 0 && dz == 0) {
                    continue;
                }
                const PathType near = fromState(world, x + dx, y + dy, z + dz);
                if (near == PathType::Damaging) {
                    return PathType::DamagingInNeighbor;
                }
                if (near == PathType::Fire || near == PathType::Lava) {
                    return PathType::FireInNeighbor;
                }
                if (near == PathType::Water) {
                    return PathType::WaterBorder;
                }
                if (near == PathType::DamageCautious) {
                    return PathType::DamageCautious;
                }
            }
        }
    }
    return type;
}

PathType flying(const world::WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t minY,
                const std::optional<std::array<std::int32_t, 3>>& mobAt) {
    PathType type = fromState(world, x, y, z);
    if (type == PathType::Open && y >= minY + 1) {
        const PathType below = fromState(world, x, y - 1, z);
        if (below == PathType::Fire || below == PathType::Lava) {
            type = PathType::Fire;
        } else if (below == PathType::Damaging) {
            type = PathType::Damaging;
        } else if (below == PathType::Cocoa) {
            type = PathType::Cocoa;
        } else if (below == PathType::Fence) {
            if (!mobAt.has_value() || *mobAt != std::array<std::int32_t, 3>{x, y - 1, z}) {
                type = PathType::Fence;
            }
        } else {
            type = below != PathType::Walkable && below != PathType::Open && below != PathType::Water
                       ? PathType::Walkable
                       : PathType::Open;
        }
    }
    if (type == PathType::Walkable || type == PathType::Open) {
        type = checkNeighbours(world, x, y, z, type);
    }
    return type;
}

} // namespace basalt::server::runtime::entity::pathTypes
