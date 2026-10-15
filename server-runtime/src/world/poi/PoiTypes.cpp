#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/poi/PoiTypes.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world::poi::poiTypes {
namespace {

namespace reg = server::registry;

struct TypeFacts {
    std::string_view name;
    std::int32_t maxTickets;
    std::int32_t validRange;
    std::string_view profession;
};

constexpr std::array<TypeFacts, static_cast<std::size_t>(PoiType::kCount)> kFacts{{
    {"", 0, 0, ""},
    {"armorer", 1, 1, "armorer"},
    {"butcher", 1, 1, "butcher"},
    {"cartographer", 1, 1, "cartographer"},
    {"cleric", 1, 1, "cleric"},
    {"farmer", 1, 1, "farmer"},
    {"fisherman", 1, 1, "fisherman"},
    {"fletcher", 1, 1, "fletcher"},
    {"leatherworker", 1, 1, "leatherworker"},
    {"librarian", 1, 1, "librarian"},
    {"mason", 1, 1, "mason"},
    {"shepherd", 1, 1, "shepherd"},
    {"toolsmith", 1, 1, "toolsmith"},
    {"weaponsmith", 1, 1, "weaponsmith"},
    {"home", 1, 1, ""},
    {"meeting", 32, 6, ""},
    {"beehive", 0, 1, ""},
    {"bee_nest", 0, 1, ""},
    {"nether_portal", 0, 1, ""},
    {"lodestone", 0, 1, ""},
    {"test_instance", 0, 1, ""},
    {"lightning_rod", 0, 1, ""},
}};

struct BlockSource {
    std::string_view block;
    PoiType type;
};

// parity: `PoiTypes.bootstrap`.
constexpr std::array<BlockSource, 46> kSources{{
    {"blast_furnace", PoiType::kArmorer},
    {"smoker", PoiType::kButcher},
    {"cartography_table", PoiType::kCartographer},
    {"brewing_stand", PoiType::kCleric},
    {"composter", PoiType::kFarmer},
    {"barrel", PoiType::kFisherman},
    {"fletching_table", PoiType::kFletcher},
    {"cauldron", PoiType::kLeatherworker},
    {"lava_cauldron", PoiType::kLeatherworker},
    {"water_cauldron", PoiType::kLeatherworker},
    {"powder_snow_cauldron", PoiType::kLeatherworker},
    {"lectern", PoiType::kLibrarian},
    {"stonecutter", PoiType::kMason},
    {"loom", PoiType::kShepherd},
    {"smithing_table", PoiType::kToolsmith},
    {"grindstone", PoiType::kWeaponsmith},
    {"white_bed", PoiType::kHome},
    {"orange_bed", PoiType::kHome},
    {"magenta_bed", PoiType::kHome},
    {"light_blue_bed", PoiType::kHome},
    {"yellow_bed", PoiType::kHome},
    {"lime_bed", PoiType::kHome},
    {"pink_bed", PoiType::kHome},
    {"gray_bed", PoiType::kHome},
    {"light_gray_bed", PoiType::kHome},
    {"cyan_bed", PoiType::kHome},
    {"purple_bed", PoiType::kHome},
    {"blue_bed", PoiType::kHome},
    {"brown_bed", PoiType::kHome},
    {"green_bed", PoiType::kHome},
    {"red_bed", PoiType::kHome},
    {"black_bed", PoiType::kHome},
    {"bell", PoiType::kMeeting},
    {"beehive", PoiType::kBeehive},
    {"bee_nest", PoiType::kBeeNest},
    {"nether_portal", PoiType::kNetherPortal},
    {"lodestone", PoiType::kLodestone},
    {"test_instance_block", PoiType::kTestInstance},
    {"lightning_rod", PoiType::kLightningRod},
    {"exposed_lightning_rod", PoiType::kLightningRod},
    {"weathered_lightning_rod", PoiType::kLightningRod},
    {"oxidized_lightning_rod", PoiType::kLightningRod},
    {"waxed_lightning_rod", PoiType::kLightningRod},
    {"waxed_exposed_lightning_rod", PoiType::kLightningRod},
    {"waxed_weathered_lightning_rod", PoiType::kLightningRod},
    {"waxed_oxidized_lightning_rod", PoiType::kLightningRod},
}};

[[nodiscard]] std::vector<PoiType> buildTable() {
    std::vector<PoiType> table(static_cast<std::size_t>(std::max(reg::blocks::stateCount(), 0)), PoiType::kNone);
    for (const BlockSource& source : kSources) {
        const reg::blockStateDefinitions::Definition* definition =
            reg::blockStateDefinitions::of(wire::common::Identifier::vanilla(std::string{source.block}));
        if (definition == nullptr) {
            continue;
        }
        const bool bed = source.type == PoiType::kHome;
        for (std::int32_t offset = 0; offset < definition->stateCount(); ++offset) {
            const std::int32_t stateId = definition->firstStateId() + offset;
            if (stateId < 0 || static_cast<std::size_t>(stateId) >= table.size()) {
                continue;
            }
            if (bed) {
                const std::map<std::string, std::string> properties = definition->propertiesOf(stateId);
                const auto part = properties.find("part");
                if (part == properties.end() || part->second != "head") {
                    continue;
                }
            }
            table[static_cast<std::size_t>(stateId)] = source.type;
        }
    }
    return table;
}

[[nodiscard]] const TypeFacts& factsOf(PoiType type) {
    const auto index = static_cast<std::size_t>(type);
    return kFacts[index < kFacts.size() ? index : 0];
}

} // namespace

PoiType forState(std::int32_t stateId) {
    static const std::vector<PoiType> kTable = buildTable();
    if (stateId < 0 || static_cast<std::size_t>(stateId) >= kTable.size()) {
        return PoiType::kNone;
    }
    return kTable[static_cast<std::size_t>(stateId)];
}

std::int32_t maxTickets(PoiType type) { return factsOf(type).maxTickets; }

std::int32_t validRange(PoiType type) { return factsOf(type).validRange; }

std::string_view name(PoiType type) { return factsOf(type).name; }

std::optional<PoiType> byName(std::string_view name) {
    constexpr std::string_view kNamespace = "minecraft:";
    if (name.starts_with(kNamespace)) {
        name.remove_prefix(kNamespace.size());
    }
    for (std::size_t index = 1; index < kFacts.size(); ++index) {
        if (kFacts[index].name == name) {
            return static_cast<PoiType>(index);
        }
    }
    return std::nullopt;
}

PoiType jobSiteOf(std::string_view profession) {
    constexpr std::string_view kNamespace = "minecraft:";
    if (profession.starts_with(kNamespace)) {
        profession.remove_prefix(kNamespace.size());
    }
    for (std::size_t index = 1; index < kFacts.size(); ++index) {
        if (!kFacts[index].profession.empty() && kFacts[index].profession == profession) {
            return static_cast<PoiType>(index);
        }
    }
    return PoiType::kNone;
}

std::string_view professionAt(PoiType type) { return factsOf(type).profession; }

} // namespace basalt::server::runtime::world::poi::poiTypes
