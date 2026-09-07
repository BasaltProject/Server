#pragma once

#include <basalt/server/runtime/world/Jukeboxes.hpp>
#include <basalt/server/runtime/world/PlayerProgress.hpp>
#include <basalt/server/runtime/world/RealBiomeSpawns.hpp>
#include <basalt/server/runtime/world/RealBlockTags.hpp>
#include <basalt/server/runtime/world/RealDamageTypeTags.hpp>
#include <basalt/server/runtime/world/RealDialogs.hpp>
#include <basalt/server/runtime/world/RealEnchantments.hpp>
#include <basalt/server/runtime/world/RealEntityTypeTags.hpp>
#include <basalt/server/runtime/world/RealFluidTags.hpp>
#include <basalt/server/runtime/world/RealItemTags.hpp>
#include <basalt/server/runtime/world/RealLootTables.hpp>
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

namespace basalt::server::runtime {

/// Every read-only registry this server reads straight out of the operator's own `server.jar`.
///
/// Built once, before anything that could depend on it: no chunk generates, no player loads and no
/// command registers before this exists. Where each table comes from (loot, tags, recipes...) is
/// exactly what vanilla's own `server.jar` holds, so no jar (as in most tests, which never break a
/// block in survival) leaves every table empty rather than absent.
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

    explicit VanillaGameData(const Jar& jar, wire::common::Logger logger = wire::common::Logger{"GameData"});

    world::RealLootTables lootTables;
    /// What each biome offers the spawner, out of the operator's own jar.
    world::RealBiomeSpawns biomeSpawns;
    /// What each villager offers, out of the same jar.
    world::RealVillagerTrades villagerTrades;
    /// The song data a comparator reads off a playing jukebox.
    world::jukeboxes::SongOutputs jukeboxSongs;
    world::RealRecipes recipes;
    world::realBlockTags::TagMembers blockTags;
    world::realItemTags::TagMembers itemTags;
    world::realFluidTags::TagMembers fluidTags;
    world::realTimelineTags::TagMembers timelineTags;
    world::realDamageTypeTags::TagMembers damageTypeTags;
    world::realEntityTypeTags::TagMembers entityTags;
    world::RealAdvancements advancements;
    world::RealEnchantments enchantments;
    world::RealDialogs dialogs;
    world::RealTimelines timelines;
    world::RealTestInstances testInstances;
    /// Structure files, loaded on demand: what `/place template` puts down.
    world::StructureTemplates structureTemplates;
};

} // namespace basalt::server::runtime
