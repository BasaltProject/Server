#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/command/ArgumentTypes.hpp>
#include <basalt/server/runtime/command/Coordinates.hpp>
#include <basalt/server/runtime/command/RegistryArguments.hpp>
#include <basalt/server/runtime/command/commands/RemainingCommands.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/world/terrain/VillageStructureGenerator.hpp>
#include <basalt/server/world/terrain/decoration/PlacedFeatures.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace basalt::server::runtime::command::commands {
namespace {

namespace reg = server::registry;
namespace structure = server::world::structure;
namespace terrain = server::world::terrain;
namespace features = server::world::terrain::decoration::placedFeatures;

using structure::StructureBoundingBox;
using structure::StructureMirror;
using structure::StructurePos;
using structure::StructureRotation;
using wire::common::Identifier;
using wire::text::Component;

/// parity: vanilla's own `/place jigsaw` cap.
constexpr std::int32_t kMaxJigsawDepth = 20;

[[nodiscard]] CommandSyntaxError unloaded() {
    return CommandSyntaxError{.component = Component::translatable("argument.pos.unloaded")};
}

/// Where a `/place` acts: the position it was given, or the whole-block position of the runner.
[[nodiscard]] std::array<std::int32_t, 3> resolveOrigin(const CommandContext& context, std::string_view name) {
    const auto* coordinates = context.get<BlockCoordinates>(name);
    const std::array<double, 3> from = context.source().position();
    return coordinates != nullptr ? coordinates->resolve(from)
                                  : std::array<std::int32_t, 3>{static_cast<std::int32_t>(std::floor(from[0])),
                                                                static_cast<std::int32_t>(std::floor(from[1])),
                                                                static_cast<std::int32_t>(std::floor(from[2]))};
}

[[nodiscard]] std::string joined(const std::vector<Identifier>& ids) {
    std::string text;
    for (const Identifier& id : ids) {
        text += text.empty() ? id.toString() : ", " + id.toString();
    }
    return text;
}

/// Writes an assembly into every chunk it covers, and re-sends them. Every chunk has to be loaded.
[[nodiscard]] std::optional<CommandSyntaxError> writePieces(const CommandContext& context, world::LoadedWorld& world,
                                                            const std::vector<structure::PlacedStructurePiece>& pieces,
                                                            terrain::VillageStructureGenerator& villages) {
    std::int32_t minX = std::numeric_limits<std::int32_t>::max();
    std::int32_t minZ = std::numeric_limits<std::int32_t>::max();
    std::int32_t maxX = std::numeric_limits<std::int32_t>::min();
    std::int32_t maxZ = std::numeric_limits<std::int32_t>::min();
    for (const structure::PlacedStructurePiece& piece : pieces) {
        minX = std::min(minX, piece.boundingBox.minX);
        minZ = std::min(minZ, piece.boundingBox.minZ);
        maxX = std::max(maxX, piece.boundingBox.maxX);
        maxZ = std::max(maxZ, piece.boundingBox.maxZ);
    }
    for (std::int32_t chunkX = minX >> 4; chunkX <= (maxX >> 4); chunkX++) {
        for (std::int32_t chunkZ = minZ >> 4; chunkZ <= (maxZ >> 4); chunkZ++) {
            server::world::Chunk* chunk = world.state().chunkIfLoaded(chunkX, chunkZ);
            if (chunk == nullptr) {
                return unloaded();
            }
            villages.placeInto(*chunk, pieces, world.metadata().seed);
        }
    }
    context.environment().syncChunks(world, minX, minZ, maxX, maxZ);
    return std::nullopt;
}

} // namespace

PlaceCommand::PlaceCommand(world::StructureTemplates& templates, std::function<std::int64_t()> nextSeed)
    : Command("place", permissionLevel::kGamemasters), templates_(&templates), nextSeed_(std::move(nextSeed)) {}

void PlaceCommand::build(const CommandNodeBuilder& root) const {
    const auto placeTemplate = [this](const CommandContext& context, bool positioned, bool rotated,
                                      bool mirrored) -> Parsed<std::int32_t> {
        const auto* id = context.get<Identifier>("template");
        const structure::StructureTemplate* found = id == nullptr ? nullptr : templates_->find(*id);
        if (found == nullptr) {
            return std::unexpected{CommandSyntaxError{
                .component =
                    Component::translatable("commands.place.template.invalid",
                                            {Component::text(id == nullptr ? std::string{} : id->toString())})}};
        }
        const Parsed<world::LoadedWorld*> world = context.world();
        if (!world.has_value()) {
            return std::unexpected{world.error()};
        }
        const auto* asked = context.get<float>("integrity");
        if (asked != nullptr && *asked < 1.0F) {
            context.source().sendSystemMessage(
                Component::text("Integrity is not modelled yet: the whole template goes in.", wire::text::kGray));
        }
        const auto* rotationArgument = rotated ? context.get<StructureRotation>("rotation") : nullptr;
        const auto* mirrorArgument = mirrored ? context.get<StructureMirror>("mirror") : nullptr;
        const StructureRotation rotation = rotationArgument == nullptr ? StructureRotation::None : *rotationArgument;
        const StructureMirror mirror = mirrorArgument == nullptr ? StructureMirror::None : *mirrorArgument;
        const std::array<std::int32_t, 3> at = resolveOrigin(context, positioned ? "pos" : "");

        const StructurePos origin{at[0], at[1], at[2]};
        const StructurePos pivot{0, 0, 0};
        const StructureBoundingBox box = found->getBoundingBox(origin, rotation, pivot, mirror);
        std::int32_t chunks = 0;
        // One pass per intersecting chunk, which is what generation does: each pass keeps only the
        // blocks that land inside that chunk.
        for (std::int32_t chunkX = box.minX >> 4; chunkX <= (box.maxX >> 4); chunkX++) {
            for (std::int32_t chunkZ = box.minZ >> 4; chunkZ <= (box.maxZ >> 4); chunkZ++) {
                server::world::Chunk* chunk = (*world)->state().chunkIfLoaded(chunkX, chunkZ);
                if (chunk == nullptr) {
                    return std::unexpected{unloaded()};
                }
                found->placeInto(*chunk,
                                 structure::TemplatePlacement{
                                     .position = origin,
                                     .rotation = rotation,
                                     .mirror = mirror,
                                     .pivot = pivot,
                                     .clipTo = StructureBoundingBox{chunkX << 4, box.minY, chunkZ << 4,
                                                                    (chunkX << 4) + 15, box.maxY, (chunkZ << 4) + 15},
                                     .ignoreStructureBlocks = true});
                chunks++;
            }
        }
        context.environment().syncChunks(**world, box.minX, box.minZ, box.maxX, box.maxZ);
        context.success(
            Component::translatable("commands.place.template.success",
                                    {Component::text(id->toString()), Component::text(std::to_string(at[0])),
                                     Component::text(std::to_string(at[1])), Component::text(std::to_string(at[2]))}),
            true);
        return chunks;
    };

    /// parity: `PlaceCommand.placeFeature`, which runs the feature once from the world's own random.
    const auto placeFeature = [this](const CommandContext& context, bool positioned) -> Parsed<std::int32_t> {
        const auto* id = context.get<Identifier>("feature");
        const Parsed<world::LoadedWorld*> world = context.world();
        if (!world.has_value()) {
            return std::unexpected{world.error()};
        }
        const features::Placement* placement = id == nullptr ? nullptr : features::find(*id);
        const features::ContextualPlacement* contextual = id == nullptr ? nullptr : features::contextual(*id);
        if (placement == nullptr && contextual == nullptr) {
            std::vector<Identifier> known{features::ids().begin(), features::ids().end()};
            std::ranges::sort(known, {}, [](const Identifier& one) { return one.path(); });
            return std::unexpected{CommandSyntaxError{
                .component = Component::text(
                    "'" + (id == nullptr ? std::string{} : id->toString()) +
                    "' has no positional body in this build, so generation can only run it per chunk. Placeable by "
                    "id: " +
                    joined(known) + ".")}};
        }
        const std::array<std::int32_t, 3> at = resolveOrigin(context, positioned ? "pos" : "");
        server::world::Chunk* chunk = (*world)->state().chunkIfLoaded(at[0] >> 4, at[2] >> 4);
        if (chunk == nullptr) {
            return std::unexpected{unloaded()};
        }

        random::RandomSource random =
            random::RandomSource::worldgen(random::RandomSource::xoroshiro(nextSeed_ ? nextSeed_() : 0));
        if (placement != nullptr) {
            (*placement)(*chunk, random, at[0], at[1], at[2]);
        } else {
            // Features that need more than a chunk: the End's own, fossils, the freeze pass and
            // template-backed sulfur roots. Refuse instead of half-placing one without its context.
            const features::FeatureContext built{
                .seed = (*world)->metadata().seed,
                .surface = (*world)->surfaceEnvironment(),
                .templates = [this](const Identifier& templateId) { return templates_->find(templateId); },
                .blockStateAt =
                    [&world](std::int32_t x, std::int32_t y, std::int32_t z) {
                        return (*world)->state().blockState(x, y, z).value_or(reg::blocks::air().stateId);
                    }};
            if (!(*contextual)(*chunk, random, at[0], at[1], at[2], built)) {
                return std::unexpected{CommandSyntaxError{
                    .component = Component::text(
                        "'" + id->toString() + "' needs something world '" + (*world)->name() +
                        "' does not have (its noise router, its surface rules or the structure files from a "
                        "downloaded server.jar).")}};
            }
        }
        context.environment().syncChunks(**world, at[0], at[2], at[0], at[2]);
        context.success(
            Component::translatable("commands.place.feature.success",
                                    {Component::text(id->toString()), Component::text(std::to_string(at[0])),
                                     Component::text(std::to_string(at[1])), Component::text(std::to_string(at[2]))}),
            true);
        return 1;
    };

    /// parity: `PlaceCommand.placeStructure`, which generates the structure where it was told and
    /// skips the placement grid and the biome check that decide where one appears on its own.
    const auto placeStructure = [](const CommandContext& context, bool positioned) -> Parsed<std::int32_t> {
        const auto* id = context.get<Identifier>("structure");
        const Parsed<world::LoadedWorld*> world = context.world();
        if (!world.has_value()) {
            return std::unexpected{world.error()};
        }
        terrain::VillageStructureGenerator* villages = (*world)->villageGenerator();
        if (villages == nullptr) {
            return std::unexpected{CommandSyntaxError{
                .component =
                    Component::translatable("commands.place.structure.invalid",
                                            {Component::text(id == nullptr ? std::string{} : id->toString())})}};
        }
        const std::vector<Identifier>& known = terrain::VillageStructureGenerator::structureIds();
        if (id == nullptr || std::ranges::find(known, *id) == known.end()) {
            return std::unexpected{CommandSyntaxError{
                .component = Component::text("'" + (id == nullptr ? std::string{} : id->toString()) +
                                             "' is not a structure this build generates. Available: " + joined(known) +
                                             ". `/place template` places a structure file.")}};
        }
        const std::array<std::int32_t, 3> at = resolveOrigin(context, positioned ? "pos" : "");
        const std::optional<std::vector<structure::PlacedStructurePiece>> pieces =
            villages->assembleAt(*id, at[0] >> 4, at[2] >> 4, (*world)->metadata().seed);
        if (!pieces.has_value()) {
            return std::unexpected{
                CommandSyntaxError{.component = Component::translatable("commands.place.structure.failed",
                                                                        {Component::text(id->toString())})}};
        }
        if (const std::optional<CommandSyntaxError> failure = writePieces(context, **world, *pieces, *villages);
            failure.has_value()) {
            return std::unexpected{*failure};
        }
        context.success(
            Component::translatable("commands.place.structure.success",
                                    {Component::text(id->toString()), Component::text(std::to_string(at[0])),
                                     Component::text(std::to_string(at[1])), Component::text(std::to_string(at[2]))}),
            true);
        return 1;
    };

    /// parity: `PlaceCommand.placeJigsaw`, which grows a template pool at a position with no
    /// structure around it.
    const auto placeJigsaw = [](const CommandContext& context, bool positioned) -> Parsed<std::int32_t> {
        const auto* pool = context.get<Identifier>("pool");
        const Parsed<world::LoadedWorld*> world = context.world();
        if (!world.has_value()) {
            return std::unexpected{world.error()};
        }
        terrain::VillageStructureGenerator* villages = (*world)->villageGenerator();
        const std::string named = pool == nullptr ? std::string{} : pool->toString();
        if (villages == nullptr || pool == nullptr || !villages->hasPool(named)) {
            return std::unexpected{CommandSyntaxError{
                .component = Component::translatable("commands.place.jigsaw.invalid", {Component::text(named)})}};
        }
        const std::array<std::int32_t, 3> at = resolveOrigin(context, positioned ? "position" : "");
        const auto* depth = context.get<std::int32_t>("max_depth");
        const std::vector<structure::PlacedStructurePiece> pieces = villages->assemblePool(
            named, at[0] >> 4, at[2] >> 4, depth == nullptr ? 1 : *depth, (*world)->metadata().seed);
        if (pieces.empty()) {
            return std::unexpected{CommandSyntaxError{
                .component = Component::translatable("commands.place.jigsaw.failed", {Component::text(named)})}};
        }
        if (const std::optional<CommandSyntaxError> failure = writePieces(context, **world, pieces, *villages);
            failure.has_value()) {
            return std::unexpected{*failure};
        }
        context.success(
            Component::translatable("commands.place.jigsaw.success",
                                    {Component::text(std::to_string(at[0])), Component::text(std::to_string(at[1])),
                                     Component::text(std::to_string(at[2]))}),
            true);
        return 1;
    };

    root.literal("feature", [placeFeature](const CommandNodeBuilder& form) {
        form.argument(
            "feature", resourceKeyArgument("minecraft:worldgen/placed_feature"),
            [placeFeature](const CommandNodeBuilder& named) {
                named.suggestsFromServer([](const SuggestionContext& context) {
                    return context.matchingIds(std::vector<Identifier>{features::ids().begin(), features::ids().end()});
                });
                named.executes([placeFeature](const CommandContext& context) { return placeFeature(context, false); });
                named.argument("pos", blockPosArgument(), [placeFeature](const CommandNodeBuilder& pos) {
                    pos.executes([placeFeature](const CommandContext& context) { return placeFeature(context, true); });
                });
            });
    });

    root.literal("structure", [placeStructure](const CommandNodeBuilder& form) {
        form.argument("structure", resourceKeyArgument("minecraft:worldgen/structure"),
                      [placeStructure](const CommandNodeBuilder& named) {
                          named.suggestsFromServer([](const SuggestionContext& context) {
                              return context.matchingIds(terrain::VillageStructureGenerator::structureIds());
                          });
                          named.executes([placeStructure](const CommandContext& context) {
                              return placeStructure(context, false);
                          });
                          named.argument("pos", blockPosArgument(), [placeStructure](const CommandNodeBuilder& pos) {
                              pos.executes([placeStructure](const CommandContext& context) {
                                  return placeStructure(context, true);
                              });
                          });
                      });
    });

    root.literal("jigsaw", [this, placeJigsaw](const CommandNodeBuilder& form) {
        form.argument(
            "pool", resourceKeyArgument("minecraft:worldgen/template_pool"),
            [this, placeJigsaw](const CommandNodeBuilder& pool) {
                pool.suggestsFromServer(
                    [this](const SuggestionContext& context) { return context.matchingIds(templates_->poolIds()); });
                pool.argument("target", resourceLocationArgument(), [placeJigsaw](const CommandNodeBuilder& target) {
                    target.argument(
                        "max_depth", integerArgument(1, kMaxJigsawDepth),
                        [placeJigsaw](const CommandNodeBuilder& depth) {
                            depth.executes(
                                [placeJigsaw](const CommandContext& context) { return placeJigsaw(context, false); });
                            depth.argument("position", blockPosArgument(),
                                           [placeJigsaw](const CommandNodeBuilder& position) {
                                               position.executes([placeJigsaw](const CommandContext& context) {
                                                   return placeJigsaw(context, true);
                                               });
                                           });
                        });
                });
            });
    });

    root.literal("template", [this, placeTemplate](const CommandNodeBuilder& form) {
        form.argument("template", resourceLocationArgument(), [this, placeTemplate](const CommandNodeBuilder& named) {
            named.suggestsFromServer(
                [this](const SuggestionContext& context) { return context.matchingIds(templates_->templateIds()); });
            named.executes(
                [placeTemplate](const CommandContext& context) { return placeTemplate(context, false, false, false); });
            named.argument("pos", blockPosArgument(), [placeTemplate](const CommandNodeBuilder& pos) {
                pos.executes([placeTemplate](const CommandContext& context) {
                    return placeTemplate(context, true, false, false);
                });
                pos.argument(
                    "rotation", templateRotationArgument(), [placeTemplate](const CommandNodeBuilder& rotation) {
                        rotation.executes([placeTemplate](const CommandContext& context) {
                            return placeTemplate(context, true, true, false);
                        });
                        rotation.argument(
                            "mirror", templateMirrorArgument(), [placeTemplate](const CommandNodeBuilder& mirror) {
                                mirror.executes([placeTemplate](const CommandContext& context) {
                                    return placeTemplate(context, true, true, true);
                                });
                                mirror.argument(
                                    "integrity", floatArgument(0.0F, 1.0F),
                                    [placeTemplate](const CommandNodeBuilder& integrity) {
                                        integrity.executes([placeTemplate](const CommandContext& context) {
                                            return placeTemplate(context, true, true, true);
                                        });
                                        integrity.literal("strict", [placeTemplate](const CommandNodeBuilder& strict) {
                                            strict.executes([placeTemplate](const CommandContext& context) {
                                                return placeTemplate(context, true, true, true);
                                            });
                                        });
                                    });
                            });
                    });
            });
        });
    });
}

} // namespace basalt::server::runtime::command::commands
