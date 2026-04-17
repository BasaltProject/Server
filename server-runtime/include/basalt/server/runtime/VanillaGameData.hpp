#pragma once

#include <basalt/server/runtime/world/BlockTransformers.hpp>
#include <basalt/server/runtime/world/Jukeboxes.hpp>
#include <basalt/server/runtime/world/LootRegistries.hpp>
#include <basalt/server/runtime/world/PlayerProgress.hpp>
#include <basalt/server/runtime/world/RealBannerPatterns.hpp>
#include <basalt/server/runtime/world/RealBiomeSpawns.hpp>
#include <basalt/server/runtime/world/RealBlockTags.hpp>
#include <basalt/server/runtime/world/RealDamageTypeTags.hpp>
#include <basalt/server/runtime/world/RealDialogs.hpp>
#include <basalt/server/runtime/world/RealEnchantmentTags.hpp>
#include <basalt/server/runtime/world/RealEnchantments.hpp>
#include <basalt/server/runtime/world/RealEntityTypeTags.hpp>
#include <basalt/server/runtime/world/RealFluidTags.hpp>
#include <basalt/server/runtime/world/RealItemTags.hpp>
#include <basalt/server/runtime/world/RealLootTables.hpp>
#include <basalt/server/runtime/world/RealPotionTags.hpp>
#include <basalt/server/runtime/world/RealRecipes.hpp>
#include <basalt/server/runtime/world/RealTestInstances.hpp>
#include <basalt/server/runtime/world/RealTimelineTags.hpp>
#include <basalt/server/runtime/world/RealTimelines.hpp>
#include <basalt/server/runtime/world/RealVillagerTrades.hpp>
#include <basalt/server/runtime/world/StructureTemplates.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::assets {
class ServerJarAssets;
}

namespace basalt::server::runtime::datapack {
class ResourceStack;
struct PackFiles;
} // namespace basalt::server::runtime::datapack

namespace basalt::server::runtime {

/// Every read-only registry this server reads straight out of the operator's own `server.jar`.
/// Built once, before any chunk generates, player loads or command registers. With no jar (most
/// tests) every table is empty rather than absent.
class VanillaGameData {
public:
    /// The three ways anything here reads the jar. Callbacks rather than the jar itself, because
    /// that is what every one of these registries already takes and because a test has no jar.
    struct Jar {
        std::function<std::optional<std::string>(std::string_view name)> readText;
        std::function<std::vector<std::string>(std::string_view prefix)> entriesUnder;
        std::function<std::optional<std::vector<std::uint8_t>>(std::string_view name)> readEntry;
        std::function<std::vector<wire::common::Identifier>(std::string_view directory, std::string_view extension)>
            dataIds;
    };

    /// The jar as callbacks over the real archive, or an empty one when there is none.
    [[nodiscard]] static Jar jarOf(const assets::ServerJarAssets* assets);

    /// The whole unpacked jar as the pack repository lists it, empty when there is none.
    [[nodiscard]] static datapack::PackFiles jarFilesOf(const assets::ServerJarAssets* assets);

    /// The same callbacks over the selected data packs, which is what everything here reads.
    [[nodiscard]] static Jar jarOf(std::shared_ptr<const datapack::ResourceStack> resources);

    explicit VanillaGameData(const Jar& jar, wire::common::Logger logger = wire::common::Logger{"GameData"});

    /// parity: `ReloadableServerResources` plus the tags `TagLoader.loadTagsForExistingRegistries`
    /// reloads: loot tables, predicates and item modifiers, recipes, advancements, every tag and the
    /// structure templates. Enchantments, dialogs, trades, timelines and the rest stay as the world
    /// loaded them, as vanilla's world registries do.
    void reload(const Jar& jar, const wire::common::Logger& logger = wire::common::Logger{"GameData"});

    world::RealLootTables lootTables;
    /// What each biome offers the spawner, out of the operator's own jar.
    world::RealBiomeSpawns biomeSpawns;
    /// What each villager offers, out of the same jar.
    world::RealVillagerTrades villagerTrades;
    /// What a hoe, a shovel or an axe turns a block into.
    world::RealBlockTransformers blockTransformers;
    /// The song data a comparator reads off a playing jukebox.
    world::jukeboxes::SongOutputs jukeboxSongs;
    world::RealRecipes recipes;
    /// The potion tags a brewing recipe's `#tag` resolves through.
    world::realPotionTags::TagMembers potionTags;
    world::realBlockTags::TagMembers blockTags;
    world::realItemTags::TagMembers itemTags;
    world::realFluidTags::TagMembers fluidTags;
    world::realTimelineTags::TagMembers timelineTags;
    world::realDamageTypeTags::TagMembers damageTypeTags;
    world::realEntityTypeTags::TagMembers entityTags;
    /// The banner-pattern tags, kept in their own order because a loom's buttons are numbered by it.
    world::realBannerPatterns::TagOrder bannerPatterns;
    world::RealAdvancements advancements;
    world::RealEnchantments enchantments;
    /// Which enchantments will not sit beside each other, which is what an anvil asks. Declared
    /// after the registry it is resolved against, because that is the order they are built in.
    world::realEnchantmentTags::TagMembers enchantmentTags;
    /// The enchantments and ordered tags a loot roll draws from.
    world::LootRegistries lootRegistries;
    world::RealDialogs dialogs;
    world::RealTimelines timelines;
    world::RealTestInstances testInstances;
    /// Structure files, loaded on demand: what `/place template` puts down.
    world::StructureTemplates structureTemplates;

private:
    void useBrewingRecipes() const;
};

} // namespace basalt::server::runtime
