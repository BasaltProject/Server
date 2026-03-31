#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/config/WorldsConfig.hpp>
#include <basalt/server/runtime/scoreboard/Scoreboard.hpp>
#include <basalt/server/runtime/world/FluidFlow.hpp>
#include <basalt/server/runtime/world/Fluids.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/LootRegistries.hpp>
#include <basalt/server/runtime/world/NaturalSpawner.hpp>
#include <basalt/server/runtime/world/RealLootTables.hpp>
#include <basalt/server/runtime/world/WorldLight.hpp>
#include <basalt/server/world/loot/LootTableJson.hpp>
#include <basalt/server/world/structure/StructureQueries.hpp>

#include <algorithm>
#include <expected>
#include <map>
#include <span>
#include <string>

namespace basalt::server::runtime::world {
namespace {

using server::world::loot::LootTable;
using wire::common::Identifier;

constexpr std::string_view kPrefix = "data/minecraft/loot_table/";
constexpr std::string_view kJsonSuffix = ".json";

[[nodiscard]] std::string stripped(const std::string& entry) {
    std::string path = entry;
    if (path.starts_with(kPrefix)) {
        path.erase(0, kPrefix.size());
    }
    if (path.ends_with(kJsonSuffix)) {
        path.resize(path.size() - kJsonSuffix.size());
    }
    return path;
}

/// The block a wall-mounted one drops as, or nothing when it is not one of them. parity:
/// `Blocks.wallVariant` gives it the standing block's loot table, so the jar carries no
/// `blocks/redstone_wall_torch.json` at all. Dropping the `wall_` segment is the whole mapping.
[[nodiscard]] std::optional<std::string> standingForm(const Identifier& block) {
    if (block.namespaceName() != Identifier::kVanillaNamespace) {
        return std::nullopt;
    }
    const std::string& path = block.path();
    const std::size_t at = path.starts_with("wall_") ? 0 : path.find("_wall_");
    if (at == std::string::npos) {
        return std::nullopt;
    }
    std::string standing = path;
    standing.erase(at == 0 ? 0 : at + 1, std::string_view{"wall_"}.size());
    return standing;
}

/// parity: `BlockState.getFluidState`: a source is `water` or `lava`, anything else flowing with its
/// amount and whether it falls.
void fluidOf(std::int32_t state, server::world::loot::PlaceFacts& place) {
    const Submersion kind = fluids::of(state);
    if (kind == Submersion::None) {
        return;
    }
    const std::string name = kind == Submersion::Water ? "water" : "lava";
    if (isSourceFluid(state)) {
        place.fluid = Identifier::vanilla(name);
        place.fluidProperties = {{"falling", "false"}};
        return;
    }
    place.fluid = Identifier::vanilla("flowing_" + name);
    const auto level = place.properties.find("level");
    const std::int32_t blockLevel = level == place.properties.end() ? 0 : std::stoi(level->second);
    const bool falling = blockLevel >= 8;
    place.fluidProperties = {{"falling", falling ? "true" : "false"},
                             {"level", std::to_string(falling ? 8 : 8 - blockLevel)}};
}

[[nodiscard]] bool litCampfire(std::int32_t state) {
    const Identifier* block = server::registry::blockStateDefinitions::blockOf(state);
    if (block == nullptr || (block->path() != "campfire" && block->path() != "soul_campfire")) {
        return false;
    }
    const std::map<std::string, std::string> properties = server::registry::blockStateDefinitions::propertiesOf(state);
    const auto lit = properties.find("lit");
    return lit != properties.end() && lit->second == "true";
}

/// parity: `CampfireBlock.isSmokeyPos`: a lit campfire up to five below, the smoke stopped by any
/// collision crossing the middle four pixels of the column.
[[nodiscard]] bool smokeyAt(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    constexpr std::int32_t kReach = 5;
    constexpr double kPostMin = 6.0 / 16.0;
    constexpr double kPostMax = 10.0 / 16.0;
    constexpr std::size_t kBoxStride = 6;
    for (std::int32_t down = 1; down <= kReach; ++down) {
        const std::int32_t state = world.blockState(x, y - down, z).value_or(0);
        if (litCampfire(state)) {
            return true;
        }
        const std::span<const std::int32_t> boxes = server::registry::blockShapes::collisionBoxes(state);
        bool blocked = false;
        for (std::size_t box = 0; box + kBoxStride <= boxes.size(); box += kBoxStride) {
            const std::span<const std::int32_t> one = boxes.subspan(box, kBoxStride);
            const auto unit = [&one](std::size_t index) {
                return static_cast<double>(one[index]) / server::registry::blockShapes::kUnits;
            };
            if (unit(0) < kPostMax && unit(3) > kPostMin && unit(2) < kPostMax && unit(5) > kPostMin &&
                unit(4) > unit(1)) {
                blocked = true;
                break;
            }
        }
        if (blocked) {
            return litCampfire(world.blockState(x, y - down - 1, z).value_or(0));
        }
    }
    return false;
}

} // namespace

// parity: 26.3 loot data names `predicate`, `item_modifier` and the two `context_*_provider`
// registries by id, and those live beside the tables in the same jar.
RealLootTables::RealLootTables(Jar jar, wire::common::Logger logger)
    : jar_(std::move(jar)), logger_(std::move(logger)),
      references_(
          [read = jar_.readText](std::string_view registry, const Identifier& id) -> std::optional<std::string> {
              if (!read) {
                  return std::nullopt;
              }
              return read("data/" + std::string{id.namespaceName()} + "/" + std::string{registry} + "/" + id.path() +
                          ".json");
          }) {}

const LootTable* RealLootTables::byId(const Identifier& id) {
    return read(id, std::string{kPrefix} + id.path() + std::string{kJsonSuffix}, "rolls nothing");
}

const LootTable* RealLootTables::forEntity(const Identifier& type) {
    return read(Identifier::vanilla("entities/" + type.path()),
                std::string{kPrefix} + "entities/" + type.path() + std::string{kJsonSuffix}, "rolls nothing");
}

const LootTable* RealLootTables::forBlock(const Identifier& block) {
    const std::optional<std::string> standing = standingForm(block);
    const std::string& path = standing.value_or(block.path());
    return read(standing.has_value() ? Identifier::vanilla(path) : block,
                std::string{kPrefix} + "blocks/" + path + std::string{kJsonSuffix}, "drops nothing");
}

void RealLootTables::prepare(server::world::loot::LootContext& context) {
    if (!context.tables) {
        context.tables = [this](const Identifier& id) { return byId(id); };
    }
    if (scoreboard_ != nullptr && !context.score) {
        context.score = [scoreboard = scoreboard_](std::string_view holder, std::string_view objective) {
            return scoreboard->score(holder, objective);
        };
    }
    if (registries_ == nullptr) {
        return;
    }
    context.enchanting = &registries_->enchanting();
    context.tagMembers = [registries = registries_](std::string_view registry, const Identifier& tag) {
        return registries->tagMembers(registry, tag);
    };
}

void RealLootTables::prepareAt(server::world::loot::LootContext& context, LoadedWorld& world, double x, double y,
                               double z) {
    prepare(context);
    context.origin = std::array<double, 3>{x, y, z};
    if (explorationMaps_) {
        context.explorationMap = [this, &world, x, y, z](server::world::loot::ItemStack& stack,
                                                         const server::world::loot::ExplorationMapRequest& request) {
            explorationMaps_(world, stack, request, {x, y, z});
        };
    }
    context.placeAt = [this, &world](std::int32_t bx, std::int32_t by, std::int32_t bz) {
        server::world::loot::PlaceFacts place;
        place.loaded = world.state().chunkIfLoaded(bx >> 4, bz >> 4) != nullptr;
        if (const std::expected<Identifier, wire::common::IdentifierError> dimension =
                Identifier::parse(config::dimensionRegistryKey(world.declaration().dimension));
            dimension.has_value()) {
            place.dimension = *dimension;
        }
        place.biome = naturalSpawner::biomeAt(world, bx, by, bz);
        if (const std::optional<std::int32_t> state = world.state().blockState(bx, by, bz); state.has_value()) {
            if (const Identifier* block = server::registry::blockStateDefinitions::blockOf(*state); block != nullptr) {
                place.block = *block;
            }
            place.properties = server::registry::blockStateDefinitions::propertiesOf(*state);
            fluidOf(*state, place);
        }
        const bool sky = config::dimensionFacts(world.declaration().dimension).hasSkylight;
        place.light = worldLight::rawBrightness(world.state(), bx, by, bz, sky, world.skyDarken());
        place.canSeeSky = naturalSpawner::canSeeSky(world, bx, by, bz);
        place.smokey = smokeyAt(world.state(), bx, by, bz);
        const LootRegistries* registries = registries_;
        place.inStructure = [&world, bx, by, bz, registries](const std::vector<std::string>& wanted) {
            const server::world::structure::StructureQueries structures{
                [&world](std::int32_t chunkX, std::int32_t chunkZ) -> const server::world::structure::ChunkStructures* {
                    const server::world::Chunk* chunk = world.state().chunkIfLoaded(chunkX, chunkZ);
                    return chunk == nullptr ? nullptr : &chunk->structures();
                }};
            const server::world::structure::StructurePos at{bx, by, bz};
            std::vector<Identifier> ids;
            for (const std::string& one : wanted) {
                const bool tag = one.starts_with('#');
                const std::expected<Identifier, wire::common::IdentifierError> id =
                    Identifier::parse(tag ? std::string_view{one}.substr(1) : std::string_view{one});
                if (!id.has_value()) {
                    continue;
                }
                if (!tag) {
                    ids.push_back(*id);
                } else if (registries != nullptr) {
                    for (const Identifier& member : registries->tagMembers("worldgen/structure", *id)) {
                        ids.push_back(member);
                    }
                }
            }
            return std::ranges::any_of(ids, [&structures, &at](const Identifier& id) {
                return structures.startWithPieceAt(at, id.path()) != nullptr;
            });
        };
        place.fluidTagged = [registries](const Identifier& fluid, std::string_view tag) {
            const std::expected<Identifier, wire::common::IdentifierError> id = Identifier::parse(tag);
            if (registries == nullptr || !id.has_value()) {
                return false;
            }
            const std::vector<Identifier> members = registries->tagMembers("fluid", *id);
            return std::ranges::find(members, fluid) != members.end();
        };
        return place;
    };
}

std::vector<Identifier> RealLootTables::ids() const {
    std::vector<Identifier> ids;
    if (!jar_.entriesUnder) {
        return ids;
    }
    for (const std::string& entry : jar_.entriesUnder(kPrefix)) {
        if (!entry.ends_with(kJsonSuffix)) {
            continue;
        }
        ids.push_back(Identifier::vanilla(stripped(entry)));
    }
    return ids;
}

const LootTable* RealLootTables::read(const Identifier& key, const std::string& path, std::string_view verb) {
    {
        const std::lock_guard<std::mutex> guard(mutex_);
        if (const auto found = cache_.find(key); found != cache_.end()) {
            return found->second.has_value() ? &*found->second : nullptr;
        }
    }
    // No jar at all is not a miss worth remembering: the install may still be cold.
    if (!jar_.readText) {
        return nullptr;
    }
    std::optional<LootTable> resolved;
    const std::optional<std::string> json = jar_.readText(path);
    if (!json.has_value()) {
        logger_.debug([&] { return key.toString() + " " + std::string{verb} + ": no such loot table"; });
    } else if (std::expected<LootTable, server::world::loot::LootTableParseError> parsed =
                   server::world::loot::lootTableJson::parse(*json, &references_);
               parsed.has_value()) {
        resolved = std::move(*parsed);
    } else {
        logger_.debug([&] { return key.toString() + " " + std::string{verb} + ": " + parsed.error().message; });
    }
    const std::lock_guard<std::mutex> guard(mutex_);
    // try_emplace, not insert_or_assign: a thread that lost the race already handed its table out.
    const auto entry = cache_.try_emplace(key, std::move(resolved)).first;
    return entry->second.has_value() ? &*entry->second : nullptr;
}

} // namespace basalt::server::runtime::world
