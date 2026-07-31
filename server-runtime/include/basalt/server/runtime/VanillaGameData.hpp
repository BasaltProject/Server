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
#include <basalt/server/runtime/world/RealTestEnvironments.hpp>
#include <basalt/server/runtime/world/RealTestInstances.hpp>
#include <basalt/server/runtime/world/RealTimelineTags.hpp>
#include <basalt/server/runtime/world/RealTimelines.hpp>
#include <basalt/server/runtime/world/RealTrialSpawnerConfigs.hpp>
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

class VanillaGameData {
public:
    struct Jar {
        std::function<std::optional<std::string>(std::string_view name)> readText;
        std::function<std::vector<std::string>(std::string_view prefix)> entriesUnder;
        std::function<std::optional<std::vector<std::uint8_t>>(std::string_view name)> readEntry;
        std::function<std::vector<wire::common::Identifier>(std::string_view directory, std::string_view extension)>
            dataIds;
    };

    [[nodiscard]] static Jar jarOf(const assets::ServerJarAssets* assets);

    [[nodiscard]] static datapack::PackFiles jarFilesOf(const assets::ServerJarAssets* assets);

    [[nodiscard]] static Jar jarOf(std::shared_ptr<const datapack::ResourceStack> resources);

    explicit VanillaGameData(const Jar& jar, wire::common::Logger logger = wire::common::Logger{"GameData"});

    void reload(const Jar& jar, const wire::common::Logger& logger = wire::common::Logger{"GameData"});

    world::RealLootTables lootTables;
    world::RealBiomeSpawns biomeSpawns;
    world::RealVillagerTrades villagerTrades;
    world::RealBlockTransformers blockTransformers;
    world::jukeboxes::SongOutputs jukeboxSongs;
    world::RealRecipes recipes;
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
    world::realEnchantmentTags::TagMembers enchantmentTags;
    world::LootRegistries lootRegistries;
    world::RealDialogs dialogs;
    world::RealTimelines timelines;
    world::RealTestInstances testInstances;
    world::RealTestEnvironments testEnvironments;
    world::RealTrialSpawnerConfigs trialSpawnerConfigs;
    world::StructureTemplates structureTemplates;

    /// Hands the brewing stand the brewing recipes the recipe list holds now.
    void useBrewingRecipes() const;
};

} // namespace basalt::server::runtime
