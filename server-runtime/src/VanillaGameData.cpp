#include <basalt/server/assets/ServerJarAssets.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/VanillaGameData.hpp>
#include <basalt/server/runtime/datapack/PackFiles.hpp>
#include <basalt/server/runtime/datapack/ResourceStack.hpp>
#include <basalt/server/runtime/world/BrewingStands.hpp>
#include <basalt/server/world/Chunk.hpp>

#include <utility>

namespace basalt::server::runtime {
namespace {

/// One flag per block state for every block a tag lists.
[[nodiscard]] std::vector<bool> statesOf(const world::realBlockTags::TagMembers& tags, std::string_view tag) {
    std::vector<bool> flags(static_cast<std::size_t>(registry::blocks::stateCount()), false);
    for (const wire::common::Identifier& block : world::realBlockTags::membersOf(tags, tag)) {
        if (const auto* definition = registry::blockStateDefinitions::of(block); definition != nullptr) {
            for (std::int32_t offset = 0; offset < definition->stateCount(); ++offset) {
                flags[static_cast<std::size_t>(definition->firstStateId() + offset)] = true;
            }
        } else if (const std::optional<std::int32_t> only = registry::blocks::registry().idOf(block);
                   only.has_value()) {
            flags[static_cast<std::size_t>(*only)] = true;
        }
    }
    return flags;
}

// parity: `Heightmap.Types` read these two tags, so a datapack that edits them moves the heightmaps.
void useHeightmapTags(const world::realBlockTags::TagMembers& tags) {
    const std::vector<bool> motion = statesOf(tags, "minecraft:blocks_motion_in_heightmap");
    const std::vector<bool> noLeaves = statesOf(tags, "minecraft:blocks_motion_in_heightmap_no_leaves");
    server::world::useHeightmapBlockSets(motion, noLeaves);
}

using wire::common::Identifier;

template <typename Reader>
[[nodiscard]] Reader readerOf(const VanillaGameData::Jar& jar) {
    return Reader{.entriesUnder = jar.entriesUnder, .readText = jar.readText};
}

} // namespace

VanillaGameData::Jar VanillaGameData::jarOf(const assets::ServerJarAssets* assets) {
    if (assets == nullptr) {
        return {};
    }
    return Jar{
        .readText = [assets](std::string_view name) -> std::optional<std::string> {
            const std::expected<std::string, assets::AssetError> text = assets->readText(name);
            return text.has_value() ? std::optional{*text} : std::nullopt;
        },
        .entriesUnder = [assets](std::string_view prefix) { return assets->entriesUnder(prefix); },
        .readEntry = [assets](std::string_view name) { return assets->readEntry(name); },
        .dataIds = [assets](std::string_view directory,
                            std::string_view extension) { return assets->dataIds(directory, extension); },
    };
}

datapack::PackFiles VanillaGameData::jarFilesOf(const assets::ServerJarAssets* assets) {
    if (assets == nullptr) {
        return {};
    }
    return datapack::PackFiles{
        .list = [assets] { return assets->entriesUnder(""); },
        .read = [assets](std::string_view name) { return assets->readEntry(name); },
    };
}

VanillaGameData::Jar VanillaGameData::jarOf(std::shared_ptr<const datapack::ResourceStack> resources) {
    if (resources == nullptr) {
        return {};
    }
    return Jar{
        .readText = [resources](std::string_view name) { return resources->readText(name); },
        .entriesUnder = [resources](std::string_view prefix) { return resources->entriesUnder(prefix); },
        .readEntry = [resources](std::string_view name) { return resources->read(name); },
        .dataIds = [resources](std::string_view directory,
                               std::string_view extension) { return resources->dataIds(directory, extension); },
    };
}

VanillaGameData::VanillaGameData(const Jar& jar, wire::common::Logger logger)
    : lootTables({.readText = jar.readText, .entriesUnder = jar.entriesUnder}), biomeSpawns({.readText = jar.readText}),
      villagerTrades({.readText = jar.readText}), blockTransformers(jar.readText), jukeboxSongs(jar.readText),
      recipes({.readText = jar.readText, .entriesUnder = jar.entriesUnder}),
      potionTags(world::realPotionTags::load(readerOf<world::realPotionTags::JarReader>(jar), logger)),
      blockTags(world::realBlockTags::load(readerOf<world::realBlockTags::JarReader>(jar), logger)),
      itemTags(world::realItemTags::load(readerOf<world::realItemTags::JarReader>(jar), logger)),
      fluidTags(world::realFluidTags::load(readerOf<world::realFluidTags::JarReader>(jar), logger)),
      timelineTags(world::realTimelineTags::load(readerOf<world::realTimelineTags::JarReader>(jar), logger)),
      damageTypeTags(world::realDamageTypeTags::load(readerOf<world::realDamageTypeTags::JarReader>(jar), logger)),
      entityTags(world::realEntityTypeTags::load(readerOf<world::realEntityTypeTags::JarReader>(jar), logger)),
      bannerPatterns(world::realBannerPatterns::load(readerOf<world::realBannerPatterns::JarReader>(jar), logger)),
      advancements(world::RealAdvancements::Jar{.entriesUnder = jar.entriesUnder, .readText = jar.readText}),
      enchantments({.readText = jar.readText, .entriesUnder = jar.entriesUnder}),
      enchantmentTags(world::realEnchantmentTags::load(
          readerOf<world::realEnchantmentTags::JarReader>(jar),
          [this](const Identifier& id) { return enchantments.contains(id); }, logger)),
      lootRegistries({.entriesUnder = jar.entriesUnder, .readText = jar.readText}, enchantments, logger),
      dialogs({.readText = jar.readText, .entriesUnder = jar.entriesUnder}),
      timelines({.readText = jar.readText, .entriesUnder = jar.entriesUnder}),
      testInstances({.readText = jar.readText, .entriesUnder = jar.entriesUnder}),
      structureTemplates({.readEntry = jar.readEntry, .dataIds = jar.dataIds}) {
    useHeightmapTags(blockTags);
    lootTables.useRegistries(&lootRegistries);
    villagerTrades.useLootTables(&lootTables);
    useBrewingRecipes();
    // Loud on purpose: an empty overworld timeline tag renders a sky that never moves, and nothing else would say why.
    const std::set<Identifier>& overworld =
        world::realTimelineTags::membersOf(timelineTags, Identifier::vanilla("in_overworld"));
    if (overworld.empty()) {
        logger.warn([] {
            return std::string{"No timelines are tagged for the overworld, so the sky will not move. "
                               "The server jar's data is missing or could not be read."};
        });
    }
}

void VanillaGameData::useBrewingRecipes() const {
    std::vector<server::world::crafting::Recipe> brews;
    for (const server::world::crafting::Recipe* recipe : recipes.brewing()) {
        brews.push_back(*recipe);
    }
    world::brewingStands::useRecipes(std::move(brews), [this](const Identifier& tag) {
        const std::set<Identifier>& members = world::realPotionTags::membersOf(potionTags, tag);
        return std::vector<Identifier>{members.begin(), members.end()};
    });
}

void VanillaGameData::reload(const Jar& jar, const wire::common::Logger& logger) {
    blockTags = world::realBlockTags::load(readerOf<world::realBlockTags::JarReader>(jar), logger);
    useHeightmapTags(blockTags);
    itemTags = world::realItemTags::load(readerOf<world::realItemTags::JarReader>(jar), logger);
    fluidTags = world::realFluidTags::load(readerOf<world::realFluidTags::JarReader>(jar), logger);
    timelineTags = world::realTimelineTags::load(readerOf<world::realTimelineTags::JarReader>(jar), logger);
    damageTypeTags = world::realDamageTypeTags::load(readerOf<world::realDamageTypeTags::JarReader>(jar), logger);
    entityTags = world::realEntityTypeTags::load(readerOf<world::realEntityTypeTags::JarReader>(jar), logger);
    bannerPatterns = world::realBannerPatterns::load(readerOf<world::realBannerPatterns::JarReader>(jar), logger);
    enchantmentTags = world::realEnchantmentTags::load(
        readerOf<world::realEnchantmentTags::JarReader>(jar),
        [this](const Identifier& id) { return enchantments.contains(id); }, logger);
    // Rebuilt where it stands: its own callbacks point at itself, and the loot tables at it.
    std::destroy_at(&lootRegistries);
    std::construct_at(&lootRegistries,
                      world::LootRegistries::Jar{.entriesUnder = jar.entriesUnder, .readText = jar.readText},
                      enchantments, logger);
    lootTables.reload({.readText = jar.readText, .entriesUnder = jar.entriesUnder});
    recipes = world::RealRecipes({.readText = jar.readText, .entriesUnder = jar.entriesUnder});
    potionTags = world::realPotionTags::load(readerOf<world::realPotionTags::JarReader>(jar), logger);
    useBrewingRecipes();
    advancements = world::RealAdvancements(
        world::RealAdvancements::Jar{.entriesUnder = jar.entriesUnder, .readText = jar.readText});
    structureTemplates.reload({.readEntry = jar.readEntry, .dataIds = jar.dataIds});
}

} // namespace basalt::server::runtime
