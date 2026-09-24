#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/World.hpp>
#include <basalt/server/runtime/command/RegistryArguments.hpp>
#include <basalt/server/runtime/command/commands/LocateCommand.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/world/terrain/JigsawStructureGenerator.hpp>
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
constexpr std::int32_t kSectionHeight = 16;

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
                        for (const terrain::JigsawStructureGenerator* jigsaw : world->jigsawGenerators()) {
                            ids.push_back(jigsaw->id());
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
                    } else if (terrain::JigsawStructureGenerator* jigsaw = (*world)->jigsawGenerator(*wanted);
                               jigsaw != nullptr) {
                        const std::optional<std::pair<std::int32_t, std::int32_t>> found =
                            jigsaw->locate((*world)->metadata().seed, startX >> 4, startZ >> 4);
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
                    const std::vector<Identifier> made = reg::pointsOfInterest::blocksOf(*wanted);
                    if (made.empty()) {
                        return std::unexpected{notFound("commands.locate.poi.not_found", *wanted)};
                    }
                    const std::set<Identifier> blocks{made.begin(), made.end()};
                    const auto isWanted = [&blocks](std::int32_t stateId) {
                        const std::optional<reg::BlockState> state = reg::blocks::byStateId(stateId);
                        return state.has_value() && blocks.contains(state->block);
                    };

                    const std::array<double, 3> origin = context.source().position();
                    const std::int32_t startX = rounded(origin[0]);
                    const std::int32_t startY = rounded(origin[1]);
                    const std::int32_t startZ = rounded(origin[2]);
                    std::array<std::int32_t, 3> best{0, 0, 0};
                    std::int64_t bestDistance = std::numeric_limits<std::int64_t>::max();
                    const auto consider = [&best, &bestDistance, startX, startY, startZ](std::int32_t x, std::int32_t y,
                                                                                         std::int32_t z) {
                        const auto dx = static_cast<std::int64_t>(x - startX);
                        const auto dy = static_cast<std::int64_t>(y - startY);
                        const auto dz = static_cast<std::int64_t>(z - startZ);
                        const std::int64_t distance = (dx * dx) + (dy * dy) + (dz * dz);
                        if (distance < bestDistance) {
                            bestDistance = distance;
                            best = {x, y, z};
                        }
                    };

                    for (std::int32_t chunkX = (startX - kPoiRadius) >> 4; chunkX <= ((startX + kPoiRadius) >> 4);
                         chunkX++) {
                        for (std::int32_t chunkZ = (startZ - kPoiRadius) >> 4; chunkZ <= ((startZ + kPoiRadius) >> 4);
                             chunkZ++) {
                            const server::world::Chunk* chunk = (*world)->state().chunkIfLoaded(chunkX, chunkZ);
                            if (chunk == nullptr) {
                                continue;
                            }
                            for (std::size_t index = 0; index < chunk->sectionCount(); index++) {
                                const server::world::ChunkSection& section = chunk->section(index);
                                if (!section.mayContain(isWanted)) {
                                    continue;
                                }
                                const std::int32_t sectionMinY = chunk->verticalBounds().minY() +
                                                                 (static_cast<std::int32_t>(index) * kSectionHeight);
                                for (std::int32_t localY = 0; localY < kSectionHeight; localY++) {
                                    for (std::int32_t localZ = 0; localZ < 16; localZ++) {
                                        for (std::int32_t localX = 0; localX < 16; localX++) {
                                            if (!isWanted(section.blockStateId(localX, localY, localZ))) {
                                                continue;
                                            }
                                            consider((chunkX * 16) + localX, sectionMinY + localY,
                                                     (chunkZ * 16) + localZ);
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // Nether portals are the one point of interest this build indexes, so
                    // they are found even in chunks nobody has loaded. Everything else can
                    // only be seen where the blocks are.
                    if (blocks.contains(Identifier::vanilla("nether_portal"))) {
                        if (const std::optional<std::array<std::int32_t, 3>> indexed =
                                (*world)->portals().nearest(startX, startY, startZ, kPoiRadius);
                            indexed.has_value()) {
                            consider((*indexed)[0], (*indexed)[1], (*indexed)[2]);
                        }
                    }
                    if (bestDistance == std::numeric_limits<std::int64_t>::max()) {
                        return std::unexpected{notFound("commands.locate.poi.not_found", *wanted)};
                    }
                    const std::int32_t distance = rounded(std::sqrt(static_cast<double>(bestDistance)));
                    context.success(Component::translatable(
                        "commands.locate.poi.success",
                        {Component::text(wanted->toString()),
                         Component::text(std::to_string(best[0]) + ", " + std::to_string(best[1]) + ", " +
                                         std::to_string(best[2])),
                         Component::text(std::to_string(distance))}));
                    return distance;
                });
            });
    });
}

} // namespace basalt::server::runtime::command::commands
