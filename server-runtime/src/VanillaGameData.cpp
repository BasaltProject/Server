#include <basalt/server/assets/ServerJarAssets.hpp>
#include <basalt/server/runtime/VanillaGameData.hpp>

#include <utility>

namespace basalt::server::runtime {
namespace {

using wire::common::Identifier;

/// The two-callback reader every tag namespace takes, built from the four this holds.
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

VanillaGameData::VanillaGameData(const Jar& jar, wire::common::Logger logger)
    : lootTables({.readText = jar.readText, .entriesUnder = jar.entriesUnder}), biomeSpawns({.readText = jar.readText}),
      villagerTrades({.readText = jar.readText}), jukeboxSongs(jar.readText),
      recipes({.readText = jar.readText, .entriesUnder = jar.entriesUnder}),
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
    lootTables.useRegistries(&lootRegistries);
    // Loud on purpose: a client whose overworld timeline tag is empty renders a sky that never
    // moves, and nothing else in the log would say why.
    const std::set<Identifier>& overworld =
        world::realTimelineTags::membersOf(timelineTags, Identifier::vanilla("in_overworld"));
    if (overworld.empty()) {
        logger.warn([] {
            return std::string{"No timelines are tagged for the overworld, so the sky will not move. "
                               "The server jar's data is missing or could not be read."};
        });
    }
}

} // namespace basalt::server::runtime
