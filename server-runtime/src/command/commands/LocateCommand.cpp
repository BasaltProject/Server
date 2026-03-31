#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/World.hpp>
#include <basalt/server/runtime/command/RegistryArguments.hpp>
#include <basalt/server/runtime/command/commands/LocateCommand.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/poi/PoiManager.hpp>
#include <basalt/server/world/terrain/StrongholdStructureGenerator.hpp>
#include <basalt/server/world/terrain/VillageStructureGenerator.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <set>
#include <string>
#include <vector>

namespace basalt::server::runtime::command::commands {
namespace {

namespace reg = server::registry;
namespace terrain = server::world::terrain;

using wire::common::Identifier;
using wire::text::Component;

/// parity: vanilla's own search limits for a biome: every 32 blocks, out to 6400.
constexpr std::int32_t kStepBlocks = 32;
constexpr std::int32_t kMaxRadiusBlocks = 6400;
/// parity: `PoiManager.findClosestWithType`'s own radius, in blocks.
constexpr std::int32_t kPoiRadius = 256;
/// parity: `LocateCommand`'s search, 100 cells of each placement grid.
constexpr std::int32_t kLocateCells = 100;

/// The one structure that is not placed on a grid, so it answers from its own ring list.
[[nodiscard]] const Identifier& stronghold() {
    static const Identifier id = Identifier::vanilla("stronghold");
    return id;
}

[[nodiscard]] std::int32_t rounded(double value) { return static_cast<std::int32_t>(std::llround(value)); }

[[nodiscard]] std::string at(std::int32_t x, std::int32_t z) { return std::to_string(x) + ", ~, " + std::to_string(z); }

[[nodiscard]] std::int32_t horizontalDistance(std::int32_t x, std::int32_t z, std::int32_t fromX, std::int32_t fromZ) {
    const auto dx = static_cast<double>(x - fromX);
    const auto dz = static_cast<double>(z - fromZ);
    return rounded(std::sqrt((dx * dx) + (dz * dz)));
}

[[nodiscard]] CommandSyntaxError notFound(std::string_view key, const Identifier& id) {
    return CommandSyntaxError{.component = Component::translatable(std::string{key}, {Component::text(id.toString())})};
}

} // namespace

LocateCommand::LocateCommand() : Command("locate", permissionLevel::kGamemasters) {}

void LocateCommand::build(const CommandNodeBuilder& root) const {
    root.literal("biome", [](const CommandNodeBuilder& form) {
        form.argument("biome", resourceOrTagArgument("minecraft:worldgen/biome"), [](const CommandNodeBuilder& named) {
            named.suggestsFromServer([](const SuggestionContext& context) {
                std::vector<Identifier> ids;
                for (const std::int32_t id : reg::biomes::registry().ids()) {
                    if (const Identifier* name = reg::biomes::registry().nameOf(id); name != nullptr) {
                        ids.push_back(*name);
                    }
                }
                return context.matchingIds(ids);
            });
            named.executes([](const CommandContext& context) -> Parsed<std::int32_t> {
                const Parsed<world::LoadedWorld*> world = context.world();
                if (!world.has_value()) {
                    return std::unexpected{world.error()};
                }
                const terrain::OverworldBiomeSource* source = (*world)->biomeSource();
                if (source == nullptr) {
                    return std::unexpected{CommandSyntaxError{
                        .component = Component::text("World '" + (*world)->name() +
                                                     "' has no biome source to search: only a normal "
                                                     "Overworld does.")}};
                }
                const auto* wanted = context.get<Identifier>("biome");
                if (wanted == nullptr) {
                    return 0;
                }
                const std::array<double, 3> origin = context.source().position();
                const std::int32_t startX = rounded(origin[0]);
                const std::int32_t startY = rounded(origin[1]);
                const std::int32_t startZ = rounded(origin[2]);

                // parity: `BiomeSource.findClosestBiome3d`, which steps in quarts and
                // gives up at 6400.
                for (std::int32_t radius = 0; radius <= kMaxRadiusBlocks; radius += kStepBlocks) {
                    for (std::int32_t dx = -radius; dx <= radius; dx += kStepBlocks) {
                        for (std::int32_t dz = -radius; dz <= radius; dz += kStepBlocks) {
                            // Only the ring just added, so a hit is the closest one
                            // found so far.
                            if (radius > 0 && dx * dx != radius * radius && dz * dz != radius * radius) {
                                continue;
                            }
                            const std::int32_t x = startX + dx;
                            const std::int32_t z = startZ + dz;
                            if (source->biomeAt(x, startY, z) != wanted->path()) {
                                continue;
                            }
                            const std::int32_t distance = horizontalDistance(x, z, startX, startZ);
                            context.success(
                                Component::translatable("commands.locate.biome.success",
                                                        {Component::text(wanted->toString()), Component::text(at(x, z)),
                                                         Component::text(std::to_string(distance))}));
                            return distance;
                        }
                    }
                }
                return std::unexpected{notFound("commands.locate.biome.not_found", *wanted)};
            });
        });
    });

    root.literal("structure", [](const CommandNodeBuilder& form) {
        form.argument(
            "structure", resourceOrTagKeyArgument("minecraft:worldgen/structure"), [](const CommandNodeBuilder& named) {
                named.suggestsFromServer([](const SuggestionContext& context) {
                    const world::LoadedWorld* world = context.world();
                    std::vector<Identifier> ids = world == nullptr ? terrain::VillageStructureGenerator::structureIds()
                                                                   : world->structureSetIds();
                    if (world != nullptr) {
                        for (const world::StructureLocator& locator : world->structureLocators()) {
                            ids.push_back(locator.id);
                        }
                    }
                    ids.push_back(stronghold());
                    return context.matchingIds(ids);
                });
                named.executes([](const CommandContext& context) -> Parsed<std::int32_t> {
                    const Parsed<world::LoadedWorld*> world = context.world();
                    if (!world.has_value()) {
                        return std::unexpected{world.error()};
                    }
                    const auto* wanted = context.get<Identifier>("structure");
                    if (wanted == nullptr) {
                        return 0;
                    }
                    const std::array<double, 3> origin = context.source().position();
                    const std::int32_t startX = rounded(origin[0]);
                    const std::int32_t startZ = rounded(origin[2]);

                    std::int32_t x = 0;
                    std::int32_t z = 0;
                    if (*wanted == stronghold()) {
                        // parity: the concentric rings path of `findNearestMapStructure`,
                        // which is not a search: the whole ring list is walked and the
                        // nearest chunk centre wins.
                        terrain::StrongholdStructureGenerator* strongholds = (*world)->strongholdGenerator();
                        const std::optional<std::pair<std::int32_t, std::int32_t>> found =
                            strongholds == nullptr ? std::nullopt : strongholds->locate(startX, startZ);
                        if (!found.has_value()) {
                            return std::unexpected{notFound("commands.locate.structure.not_found", stronghold())};
                        }
                        x = found->first * 16;
                        z = found->second * 16;
                    } else if (const auto locator = std::ranges::find((*world)->structureLocators(), *wanted,
                                                                      &world::StructureLocator::id);
                               locator != (*world)->structureLocators().end()) {
                        const std::optional<std::pair<std::int32_t, std::int32_t>> found =
                            locator->locate((*world)->metadata().seed, startX >> 4, startZ >> 4, kLocateCells, {});
                        if (!found.has_value()) {
                            return std::unexpected{notFound("commands.locate.structure.not_found", *wanted)};
                        }
                        x = found->first * 16;
                        z = found->second * 16;
                    } else {
                        const std::vector<Identifier> known = (*world)->structureSetIds();
                        if (known.empty()) {
                            return std::unexpected{notFound("commands.locate.structure.not_found", *wanted)};
                        }
                        terrain::VillageStructureGenerator* generator = (*world)->structureSetGenerator(*wanted);
                        if (generator == nullptr) {
                            std::string listed;
                            for (const Identifier& id : known) {
                                listed += listed.empty() ? id.toString() : ", " + id.toString();
                            }
                            return std::unexpected{CommandSyntaxError{
                                .component = Component::text(
                                    "'" + wanted->toString() +
                                    "' is not a structure this build generates. Available: " + listed)}};
                        }
                        const std::optional<terrain::LocatedVillage> found =
                            generator->locate((*world)->metadata().seed, startX >> 4, startZ >> 4, wanted);
                        if (!found.has_value()) {
                            return std::unexpected{notFound("commands.locate.structure.not_found", *wanted)};
                        }
                        // parity: `StructurePlacement.getLocatePos`, which names the
                        // chunk's minimum corner plus the set's own `locate_offset`; every
                        // set that has one leaves it at zero.
                        x = found->chunkX * 16;
                        z = found->chunkZ * 16;
                    }
                    const std::int32_t distance = horizontalDistance(x, z, startX, startZ);
                    context.success(
                        Component::translatable("commands.locate.structure.success",
                                                {Component::text(wanted->toString()), Component::text(at(x, z)),
                                                 Component::text(std::to_string(distance))}));
                    return distance;
                });
            });
    });

    /// parity: `LocateCommand.locatePoi`, which searches 256 blocks around the runner and takes the
    /// nearest.
    root.literal("poi", [](const CommandNodeBuilder& form) {
        form.argument(
            "poi", resourceOrTagArgument("minecraft:point_of_interest_type"), [](const CommandNodeBuilder& named) {
                named.suggestsFromServer(
                    [](const SuggestionContext& context) { return context.matchingIds(reg::pointsOfInterest::ids()); });
                named.executes([](const CommandContext& context) -> Parsed<std::int32_t> {
                    const Parsed<world::LoadedWorld*> world = context.world();
                    if (!world.has_value()) {
                        return std::unexpected{world.error()};
                    }
                    const auto* wanted = context.get<Identifier>("poi");
                    if (wanted == nullptr) {
                        return 0;
                    }
                    const std::optional<world::poi::PoiType> type = world::poi::poiTypes::byName(wanted->toString());
                    if (!type.has_value()) {
                        return std::unexpected{notFound("commands.locate.poi.not_found", *wanted)};
                    }
                    // parity: `BlockPos.containing` of the source and `findClosestWithType` at any occupancy.
                    const std::array<double, 3> origin = context.source().position();
                    const world::poi::BlockPos from{static_cast<std::int32_t>(std::floor(origin[0])),
                                                    static_cast<std::int32_t>(std::floor(origin[1])),
                                                    static_cast<std::int32_t>(std::floor(origin[2]))};
                    const std::optional<world::poi::BlockPos> found = (*world)->state().poi().findClosest(
                        world::poi::maskOf(*type), from, kPoiRadius, world::poi::Occupancy::kAny);
                    if (!found.has_value()) {
                        return std::unexpected{notFound("commands.locate.poi.not_found", *wanted)};
                    }
                    const auto dx = static_cast<float>(found->x - from.x);
                    const auto dz = static_cast<float>(found->z - from.z);
                    const auto distance = static_cast<std::int32_t>(std::floor(std::sqrt((dx * dx) + (dz * dz))));
                    context.success(Component::translatable("commands.locate.poi.success",
                                                            {Component::text(wanted->toString()),
                                                             Component::text(at(found->x, found->z)),
                                                             Component::text(std::to_string(distance))}));
                    return distance;
                });
            });
    });
}

} // namespace basalt::server::runtime::command::commands
