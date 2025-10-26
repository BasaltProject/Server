#include <basalt/server/random/PositionalHashing.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/structure/TemplateBlockProcessors.hpp>

#include <array>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>

namespace basalt::server::world::structure {
namespace {

constexpr float kReplaceFullBlockChance = 0.5F;
constexpr float kReplaceStairsChance = 0.5F;
constexpr float kObsidianCryingChance = 0.15F;

constexpr std::array<std::string_view, 4> kHorizontal{"north", "south", "west", "east"};
constexpr std::array<std::string_view, 2> kHalves{"bottom", "top"};

using PathSet = std::set<std::string, std::less<>>;

const PathSet& structureOnly() {
    static const PathSet paths = {"structure_void", "structure_block", "jigsaw"};
    return paths;
}

/// The blocks a feature is never allowed to overwrite. parity: `#features_cannot_replace`.
const PathSet& featuresCannotReplace() {
    static const PathSet paths = {"bedrock",       "spawner", "chest", "end_portal_frame", "reinforced_deepslate",
                                  "trial_spawner", "vault"};
    return paths;
}

const std::map<std::string, std::string, std::less<>>& blackstoneByPath() {
    static const std::map<std::string, std::string, std::less<>> replacements = {
        {"cobblestone", "blackstone"},
        {"mossy_cobblestone", "blackstone"},
        {"stone", "blackstone"},
        {"cobblestone_stairs", "blackstone_stairs"},
        {"stone_stairs", "blackstone_stairs"},
        {"mossy_cobblestone_stairs", "blackstone_stairs"},
        {"cobblestone_slab", "blackstone_slab"},
        {"stone_slab", "blackstone_slab"},
        {"mossy_cobblestone_slab", "blackstone_slab"},
        {"cobblestone_wall", "blackstone_wall"},
        {"mossy_cobblestone_wall", "blackstone_wall"}};
    return replacements;
}

[[nodiscard]] std::string pathOf(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string{} : block->path();
}

[[nodiscard]] std::int32_t state(std::string_view path) {
    const registry::BlockState* found = registry::blocks::named(path);
    return found == nullptr ? registry::blocks::air().stateId : found->stateId;
}

/// parity: `withPropertiesOf`, which carries over every property the two blocks share.
[[nodiscard]] std::int32_t keepProperties(std::int32_t input, std::string_view path) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(input);
    const std::optional<std::int32_t> found =
        registry::blocks::stateIdOf(wire::common::Identifier::vanilla(std::string{path}), properties);
    return found.value_or(state(path));
}

[[nodiscard]] std::int32_t randomFacingStairs(random::RandomSource& random, std::string_view path) {
    const std::string_view facing =
        kHorizontal[static_cast<std::size_t>(random.nextInt(static_cast<std::int32_t>(kHorizontal.size())))];
    const std::string_view half =
        kHalves[static_cast<std::size_t>(random.nextInt(static_cast<std::int32_t>(kHalves.size())))];
    const std::map<std::string, std::string> properties = {{"facing", std::string{facing}},
                                                           {"half", std::string{half}}};
    const std::optional<std::int32_t> found =
        registry::blocks::stateIdOf(wire::common::Identifier::vanilla(std::string{path}), properties);
    return found.value_or(state(path));
}

[[nodiscard]] std::int32_t pick(random::RandomSource& random, float mossiness,
                                const std::array<std::int32_t, 2>& nonMossy, const std::array<std::int32_t, 2>& mossy) {
    const std::array<std::int32_t, 2>& chosen = random.nextFloat() < mossiness ? mossy : nonMossy;
    return chosen[static_cast<std::size_t>(random.nextInt(static_cast<std::int32_t>(chosen.size())))];
}

[[nodiscard]] std::optional<std::int32_t> fullStoneBlock(random::RandomSource& random, float mossiness) {
    if (random.nextFloat() >= kReplaceFullBlockChance) {
        return std::nullopt;
    }
    const std::array<std::int32_t, 2> nonMossy{state("cracked_stone_bricks"),
                                               randomFacingStairs(random, "stone_brick_stairs")};
    const std::array<std::int32_t, 2> mossy{state("mossy_stone_bricks"),
                                            randomFacingStairs(random, "mossy_stone_brick_stairs")};
    return pick(random, mossiness, nonMossy, mossy);
}

[[nodiscard]] std::optional<std::int32_t> stairs(std::int32_t input, random::RandomSource& random, float mossiness) {
    if (random.nextFloat() >= kReplaceStairsChance) {
        return std::nullopt;
    }
    static const std::array<std::int32_t, 2> nonMossy{state("stone_slab"), state("stone_brick_slab")};
    const std::array<std::int32_t, 2> mossy{keepProperties(input, "mossy_stone_brick_stairs"),
                                            state("mossy_stone_brick_slab")};
    return pick(random, mossiness, nonMossy, mossy);
}

} // namespace

TemplateBlockProcessor templateBlockProcessors::chain(std::vector<TemplateBlockProcessor> processors) {
    return [processors = std::move(processors)](std::int32_t input, std::int32_t world,
                                                StructurePos position) -> std::optional<std::int32_t> {
        std::int32_t state = input;
        for (const TemplateBlockProcessor& processor : processors) {
            const std::optional<std::int32_t> next = processor(state, world, position);
            if (!next.has_value()) {
                return std::nullopt;
            }
            state = *next;
        }
        return state;
    };
}

TemplateBlockProcessor templateBlockProcessors::ignoreStructureBlock() {
    return [](std::int32_t input, std::int32_t, StructurePos) -> std::optional<std::int32_t> {
        return structureOnly().contains(pathOf(input)) ? std::nullopt : std::optional{input};
    };
}

TemplateBlockProcessor templateBlockProcessors::ignoreStructureAndAir() {
    return [](std::int32_t input, std::int32_t, StructurePos) -> std::optional<std::int32_t> {
        if (structureOnly().contains(pathOf(input)) || registry::blocks::isAir(input)) {
            return std::nullopt;
        }
        return input;
    };
}

TemplateBlockProcessor templateBlockProcessors::protectFeatures() {
    return [](std::int32_t input, std::int32_t world, StructurePos) -> std::optional<std::int32_t> {
        return featuresCannotReplace().contains(pathOf(world)) ? std::nullopt : std::optional{input};
    };
}

TemplateBlockProcessor templateBlockProcessors::lavaSubmerged() {
    return [](std::int32_t input, std::int32_t world, StructurePos) -> std::optional<std::int32_t> {
        // parity: `Block.isShapeFullBlock`, a full cube and not merely something solid: a slab or a
        // stair keeps the lava around it, a netherrack block displaces it.
        if (pathOf(world) == "lava" && !registry::blockShapes::isSolidRender(input)) {
            return state("lava");
        }
        return input;
    };
}

TemplateBlockProcessor templateBlockProcessors::blackstoneReplace() {
    return [](std::int32_t input, std::int32_t, StructurePos) -> std::optional<std::int32_t> {
        const auto found = blackstoneByPath().find(pathOf(input));
        if (found == blackstoneByPath().end()) {
            return input;
        }
        return keepProperties(input, found->second);
    };
}

TemplateBlockProcessor templateBlockProcessors::age(float mossiness) {
    return [mossiness](std::int32_t input, std::int32_t, StructurePos position) -> std::optional<std::int32_t> {
        random::RandomSource random = random::RandomSource::worldgen(
            random::RandomSource::legacy(random::positionalSeed(position.x, position.y, position.z)));
        const std::string path = pathOf(input);
        if (path == "stone_bricks" || path == "stone" || path == "chiseled_stone_bricks") {
            return fullStoneBlock(random, mossiness).value_or(input);
        }
        if (path.ends_with("_stairs")) {
            return stairs(input, random, mossiness).value_or(input);
        }
        if (path.ends_with("_slab")) {
            return random.nextFloat() < mossiness ? keepProperties(input, "mossy_stone_brick_slab") : input;
        }
        if (path.ends_with("_wall")) {
            return random.nextFloat() < mossiness ? keepProperties(input, "mossy_stone_brick_wall") : input;
        }
        if (path == "obsidian") {
            return random.nextFloat() < kObsidianCryingChance ? state("crying_obsidian") : input;
        }
        return input;
    };
}

} // namespace basalt::server::world::structure
