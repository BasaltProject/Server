#include <basalt/server/world/terrain/decoration/AquaticDecoration.hpp>
#include <basalt/server/world/terrain/decoration/BambooDecoration.hpp>
#include <basalt/server/world/terrain/decoration/BiomeFlowerFeatures.hpp>
#include <basalt/server/world/terrain/decoration/BushPatchDecoration.hpp>
#include <basalt/server/world/terrain/decoration/CactusFeature.hpp>
#include <basalt/server/world/terrain/decoration/CaveLocalModificationDecoration.hpp>
#include <basalt/server/world/terrain/decoration/CaveSpeleothemDecoration.hpp>
#include <basalt/server/world/terrain/decoration/CoralDecoration.hpp>
#include <basalt/server/world/terrain/decoration/DesertWellDecoration.hpp>
#include <basalt/server/world/terrain/decoration/DoublePlantPatchDecoration.hpp>
#include <basalt/server/world/terrain/decoration/FlowerFeatures.hpp>
#include <basalt/server/world/terrain/decoration/FlowerVegetation.hpp>
#include <basalt/server/world/terrain/decoration/ForestRockDecoration.hpp>
#include <basalt/server/world/terrain/decoration/FrozenOceanDecoration.hpp>
#include <basalt/server/world/terrain/decoration/GlowLichenDecoration.hpp>
#include <basalt/server/world/terrain/decoration/GrassPatchDecoration.hpp>
#include <basalt/server/world/terrain/decoration/GrassScatterPatches.hpp>
#include <basalt/server/world/terrain/decoration/GroundPlantPatchDecoration.hpp>
#include <basalt/server/world/terrain/decoration/GroundVegetation.hpp>
#include <basalt/server/world/terrain/decoration/HugeMushroomFeature.hpp>
#include <basalt/server/world/terrain/decoration/IceSpikesDecoration.hpp>
#include <basalt/server/world/terrain/decoration/JungleFeatures.hpp>
#include <basalt/server/world/terrain/decoration/MelonPatchDecoration.hpp>
#include <basalt/server/world/terrain/decoration/MushroomFieldsDecoration.hpp>
#include <basalt/server/world/terrain/decoration/MushroomPatchDecoration.hpp>
#include <basalt/server/world/terrain/decoration/OverworldOreDecoration.hpp>
#include <basalt/server/world/terrain/decoration/OverworldSpringDecoration.hpp>
#include <basalt/server/world/terrain/decoration/PaleMossDecoration.hpp>
#include <basalt/server/world/terrain/decoration/PlacedFeatures.hpp>
#include <basalt/server/world/terrain/decoration/PlainsFlowerFeatures.hpp>
#include <basalt/server/world/terrain/decoration/SculkDecoration.hpp>
#include <basalt/server/world/terrain/decoration/SmallFeatures.hpp>
#include <basalt/server/world/terrain/decoration/StandaloneFeatures.hpp>
#include <basalt/server/world/terrain/decoration/SurfacePatches.hpp>
#include <basalt/server/world/terrain/decoration/TallGrassDecoration.hpp>
#include <basalt/server/world/terrain/decoration/TreeDecoration.hpp>
#include <basalt/server/world/terrain/decoration/VinePatchDecoration.hpp>
#include <basalt/server/world/terrain/decoration/end/EndDecoration.hpp>
#include <basalt/server/world/terrain/decoration/nether/NetherLocalDecoration.hpp>
#include <basalt/server/world/terrain/decoration/nether/NetherOreDecoration.hpp>
#include <basalt/server/world/terrain/decoration/nether/NetherSurfaceStructuresDecoration.hpp>
#include <basalt/server/world/terrain/decoration/nether/NetherUndergroundDecoration.hpp>
#include <basalt/server/world/terrain/decoration/nether/NetherVegetalDecoration.hpp>
#include <basalt/server/world/terrain/decoration/tree/FallenTreeFeature.hpp>
#include <basalt/server/world/terrain/decoration/tree/TreeConfigs.hpp>
#include <basalt/server/world/terrain/decoration/tree/TreeFeature.hpp>

#include <map>
#include <string>
#include <string_view>
#include <utility>

namespace basalt::server::world::terrain::decoration {
namespace {

using placedFeatures::Placement;
using Registry = std::map<wire::common::Identifier, Placement>;

[[nodiscard]] Placement treePlacement(const tree::TreeConfiguration& config) {
    return [&config](Chunk& chunk, random::RandomSource& random, std::int32_t x, std::int32_t y, std::int32_t z) {
        static_cast<void>(tree::treeFeature::place(chunk, random, x, y, z, config));
    };
}

[[nodiscard]] Placement fallenPlacement(const tree::FallenTreeConfiguration& config) {
    return [&config](Chunk& chunk, random::RandomSource& random, std::int32_t x, std::int32_t y, std::int32_t z) {
        static_cast<void>(tree::fallenTreeFeature::place(chunk, random, x, y, z, config));
    };
}

[[nodiscard]] Placement mushroomPlacement(hugeMushroomFeature::Kind kind) {
    return [kind](Chunk& chunk, random::RandomSource& random, std::int32_t x, std::int32_t y, std::int32_t z) {
        hugeMushroomFeature::place(chunk, random, x, y, z, kind);
    };
}

/// Wraps a placement whose body takes the position as one value rather than three.
template <typename Body>
[[nodiscard]] Placement byPos(Body body) {
    return [body = std::move(body)](Chunk& chunk, random::RandomSource& random, std::int32_t x, std::int32_t y,
                                    std::int32_t z) { body(chunk, random, DecorationPos{x, y, z}); };
}

/// Wraps a named placement, binding the name the registry was keyed under.
template <typename Body>
[[nodiscard]] Placement byName(Body body, std::string name) {
    return [body = std::move(body), name = std::move(name)](Chunk& chunk, random::RandomSource& random, std::int32_t x,
                                                            std::int32_t y, std::int32_t z) {
        body(chunk, random, DecorationPos{x, y, z}, name);
    };
}

void put(Registry& registry, std::string_view name, Placement placement) {
    registry.insert_or_assign(wire::common::Identifier::vanilla(std::string{name}), std::move(placement));
}

void putTrees(Registry& registry) {
    namespace configs = tree::treeConfigs;
    const std::vector<std::pair<std::string_view, const tree::TreeConfiguration&>> trees{
        {"oak", configs::oak()},
        {"oak_checked", configs::oak()},
        {"oak_leaf_litter", configs::oakLeafLitter()},
        {"trees_swamp", configs::swampOak()},
        {"birch_checked", configs::birch()},
        {"birch_leaf_litter", configs::birchLeafLitter()},
        {"birch_tall", configs::superBirch()},
        {"super_birch_bees", configs::superBirchBees()},
        {"fancy_oak_checked", configs::fancyOak()},
        {"fancy_oak_leaf_litter", configs::fancyOakLeafLitter()},
        {"fancy_oak_bees", configs::fancyOakBees()},
        {"jungle_tree", configs::jungleTree()},
        {"jungle_bush", configs::jungleBush()},
        {"mega_jungle_tree_checked", configs::megaJungle()},
        {"spruce", configs::spruce()},
        {"spruce_checked", configs::spruce()},
        {"spruce_on_snow", configs::spruce()},
        {"mega_spruce_checked", configs::megaSpruce()},
        {"pine", configs::pine()},
        {"pine_checked", configs::pine()},
        {"pine_on_snow", configs::pine()},
        {"mega_pine_checked", configs::megaPine()},
        {"acacia", configs::acacia()},
        {"acacia_checked", configs::acacia()},
        {"dark_oak_checked", configs::darkOak()},
        {"dark_oak_leaf_litter", configs::darkOakLeafLitter()},
        {"pale_oak_checked", configs::paleOak()},
        {"pale_oak_creaking_checked", configs::paleOakCreaking()},
        {"mangrove_checked", configs::mangrove()},
        {"tall_mangrove_checked", configs::tallMangrove()},
        {"cherry_checked", configs::cherry()},
        // The bee variants are the same trees with a hive decorator, and vanilla names each.
        {"oak_bees_002", configs::oakBees002()},
        {"oak_bees_0002_leaf_litter", configs::oakBees0002LeafLitter()},
        {"birch_bees_002", configs::birchBees002()},
        {"birch_bees_0002", configs::birchBees0002()},
        {"birch_bees_0002_leaf_litter", configs::birchBees0002LeafLitter()},
        {"fancy_oak_bees_002", configs::fancyOakBees002()},
        {"fancy_oak_bees_0002_leaf_litter", configs::fancyOakBees0002LeafLitter()},
        {"super_birch_bees_0002", configs::superBirchBees0002()},
        {"cherry_bees_005", configs::cherryBees005()}};
    for (const auto& [name, config] : trees) {
        put(registry, name, treePlacement(config));
    }

    namespace fallen = tree::fallenTreeConfigs;
    const std::vector<std::pair<std::string_view, const tree::FallenTreeConfiguration&>> fallenTrees{
        {"fallen_oak_tree", fallen::oak()},
        {"fallen_birch_tree", fallen::birch()},
        {"fallen_super_birch_tree", fallen::superBirch()},
        {"fallen_spruce_tree", fallen::spruce()},
        {"fallen_jungle_tree", fallen::jungle()}};
    for (const auto& [name, config] : fallenTrees) {
        put(registry, name, fallenPlacement(config));
    }
}

void putOresAndSprings(Registry& registry) {
    for (const auto& [name, index] : overworldOreDecoration::placedOreIds()) {
        put(registry, name,
            [name](Chunk& chunk, random::RandomSource& random, std::int32_t x, std::int32_t y, std::int32_t z) {
                static_cast<void>(overworldOreDecoration::placeAt(chunk, random, x, y, z, name));
            });
    }
    for (const std::string& name : overworldOreDecoration::placedDiskIds()) {
        put(registry, name,
            [name](Chunk& chunk, random::RandomSource& random, std::int32_t x, std::int32_t y, std::int32_t z) {
                static_cast<void>(overworldOreDecoration::placeDiskAt(chunk, random, x, y, z, name));
            });
    }
    for (const std::string& name : overworldSpringDecoration::placedSpringIds()) {
        put(registry, name,
            [name](Chunk& chunk, random::RandomSource&, std::int32_t x, std::int32_t y, std::int32_t z) {
                static_cast<void>(overworldSpringDecoration::placeAt(chunk, x, y, z, name));
            });
    }
    put(registry, "underwater_magma",
        [](Chunk& chunk, random::RandomSource& random, std::int32_t x, std::int32_t y, std::int32_t z) {
            static_cast<void>(overworldOreDecoration::placeUnderwaterMagmaAt(chunk, random, x, y, z));
        });
}

void putPatches(Registry& registry) {
    put(registry, "patch_bush", byPos(bushPatchDecoration::placeAt));
    put(registry, "patch_waterlily", byPos(waterlilyPatchDecoration::placeAt));
    put(registry, "patch_pumpkin", byPos(pumpkinPatchDecoration::placeAt));
    for (const std::string_view name : {"patch_melon", "patch_melon_sparse"}) {
        put(registry, name, byPos(melonPatchDecoration::placeAt));
    }
    for (const std::string_view name : {"patch_berry_common", "patch_berry_rare", "patch_berry_bush"}) {
        put(registry, name, byPos(berryPatchDecoration::placeAt));
    }
    for (const std::string_view name : {"patch_tall_grass", "patch_tall_grass_2"}) {
        put(registry, name, byPos(tallGrassDecoration::placeAt));
    }
    put(registry, "grass_bonemeal", byName(grassPatchDecoration::placeAt, "patch_grass_plain"));
    for (const auto& [name, index] : grassPatchDecoration::placedGrassIds()) {
        put(registry, name, byName(grassPatchDecoration::placeAt, name));
    }
    for (const std::string& name : groundPlantPatchDecoration::placedIds()) {
        put(registry, name, byName(groundPlantPatchDecoration::placeAt, name));
    }
    for (const std::string& name : mushroomPatchDecoration::placedIds()) {
        put(registry, name, byName(mushroomPatchDecoration::placeAt, name));
    }
    for (const std::string& name : doublePlantPatchDecoration::placedIds()) {
        put(registry, name, byName(doublePlantPatchDecoration::placeAt, name));
    }
    for (const std::string& name : defaultVegetationDecoration::placedIds()) {
        put(registry, name, byPos(defaultVegetationDecoration::placeDefaultPatchAt));
    }
    for (const std::string& name : dryGrassDecoration::placedIds()) {
        put(registry, name, byPos(dryGrassDecoration::placeAt));
    }
    for (const std::string_view name : {"patch_dead_bush", "patch_dead_bush_2", "patch_dead_bush_badlands"}) {
        put(registry, name, byPos(desertDecoration::placeAt));
    }
    // parity: the three `patch_cactus*` placed features are the same configured feature with
    // different placement modifiers, and `/place feature` supplies the position.
    for (const std::string_view name : {"patch_cactus", "patch_cactus_desert", "patch_cactus_decorated"}) {
        put(registry, name, byPos([](Chunk& chunk, random::RandomSource& random, const DecorationPos& origin) {
                static_cast<void>(cactusFeature::place(chunk, random, origin));
            }));
    }
}

void putFlowersAndVines(Registry& registry) {
    for (const std::string& name : forestFlowerDecoration::placedIds()) {
        put(registry, name, byPos(forestFlowerDecoration::placeAt));
    }
    put(registry, "flower_meadow", byPos(meadowDecoration::placeAt));
    for (const std::string_view name : {"flower_pale_garden", "pale_garden_flowers"}) {
        put(registry, name, byPos(paleGardenDecoration::placeAt));
    }
    put(registry, "flower_cherry", byPos(cherryGroveDecoration::placeAt));
    put(registry, "flower_swamp", byPos(swampDecoration::placeAt));
    for (const std::string_view name : {"flower_plain", "flower_plains"}) {
        put(registry, name,
            [](Chunk& chunk, random::RandomSource& random, std::int32_t x, std::int32_t y, std::int32_t z) {
                plainsDecoration::placeFlowerPlainAt(chunk, random, x, y, z);
            });
    }
    put(registry, "vines", [](Chunk& chunk, random::RandomSource&, std::int32_t x, std::int32_t y, std::int32_t z) {
        static_cast<void>(jungleVineDecoration::placeAt(chunk, DecorationPos{x, y, z}));
    });
    put(registry, "glow_lichen", byPos([](Chunk& chunk, random::RandomSource& random, const DecorationPos& origin) {
            static_cast<void>(glowLichenDecoration::placeAt(chunk, random, origin));
        }));
    put(registry, "pale_moss_patch", byPos(paleMossDecoration::placeAt));
    put(registry, "mushroom_island_vegetation", byPos(mushroomFieldsDecoration::placeAt));
    for (const std::string& name : bambooDecoration::placedIds()) {
        put(registry, name, byName(bambooDecoration::placeAt, name));
    }
}

void putCavesAndOceans(Registry& registry) {
    for (const std::string& name : aquaticDecoration::placedIds()) {
        put(registry, name, byName(aquaticDecoration::placeAt, name));
    }
    for (const std::string& name : coralDecoration::placedIds()) {
        put(registry, name, byPos(coralDecoration::placeAt));
    }
    for (const std::string& name : caveSpeleothemDecoration::placedIds()) {
        put(registry, name, byName(caveSpeleothemDecoration::placeAt, name));
    }
    for (const std::string& name : sculkDecoration::placedIds()) {
        put(registry, name, byName(sculkDecoration::placeAt, name));
    }
    for (const std::string& name : iceSpikesDecoration::placedIds()) {
        put(registry, name, byName(iceSpikesDecoration::placeAt, name));
    }
    for (const std::string& name : caveLocalModificationDecoration::placedIds()) {
        put(registry, name, byName(caveLocalModificationDecoration::placeAt, name));
    }
    for (const std::string& name : frozenOceanDecoration::placedIds()) {
        put(registry, name, byName(frozenOceanDecoration::placeAt, name));
    }
    put(registry, "end_island_decorated",
        [](Chunk& chunk, random::RandomSource& random, std::int32_t x, std::int32_t y, std::int32_t z) {
            end::endRawDecoration::placeAt(chunk, random, x, y, z);
        });
}

void putNether(Registry& registry) {
    for (const std::string& name : nether::netherVegetalDecoration::placedIds()) {
        put(registry, name,
            [name](Chunk& chunk, random::RandomSource& random, std::int32_t x, std::int32_t y, std::int32_t z) {
                static_cast<void>(nether::netherVegetalDecoration::placeAt(chunk, random, x, y, z, name));
            });
    }
    for (const std::string& name : nether::netherUndergroundDecoration::placedIds()) {
        put(registry, name,
            [name](Chunk& chunk, random::RandomSource& random, std::int32_t x, std::int32_t y, std::int32_t z) {
                static_cast<void>(nether::netherUndergroundDecoration::placeAt(chunk, random, x, y, z, name));
            });
    }
    for (const std::string& name : nether::netherSurfaceStructuresDecoration::placedIds()) {
        put(registry, name,
            [name](Chunk& chunk, random::RandomSource& random, std::int32_t x, std::int32_t y, std::int32_t z) {
                static_cast<void>(nether::netherSurfaceStructuresDecoration::placeAt(chunk, random, x, y, z, name));
            });
    }
    for (const auto& [name, index] : nether::netherOreDecoration::placedIds()) {
        put(registry, name,
            [name](Chunk& chunk, random::RandomSource& random, std::int32_t x, std::int32_t y, std::int32_t z) {
                static_cast<void>(nether::netherOreDecoration::placeAt(chunk, random, x, y, z, name));
            });
    }
    put(registry, "basalt_pillar",
        [](Chunk& chunk, random::RandomSource& random, std::int32_t x, std::int32_t y, std::int32_t z) {
            static_cast<void>(nether::netherLocalDecoration::placeAt(chunk, random, x, y, z));
        });
}

void putStandalone(Registry& registry) {
    for (const auto& [name, selector] : treeDecoration::selectors()) {
        put(registry, name,
            byName(
                [](Chunk& chunk, random::RandomSource& random, const DecorationPos& origin, std::string_view feature) {
                    static_cast<void>(treeDecoration::placeAt(chunk, random, origin, feature));
                },
                name));
    }
    for (const std::string& name : smallFeatures::placedIds()) {
        put(registry, name,
            byName(
                [](Chunk& chunk, random::RandomSource& random, const DecorationPos& origin, std::string_view feature) {
                    static_cast<void>(smallFeatures::placeAt(chunk, random, origin, feature));
                },
                name));
    }
    for (const std::string& name : standaloneFeatures::placedIds()) {
        put(registry, name,
            byName(
                [](Chunk& chunk, random::RandomSource& random, const DecorationPos& origin, std::string_view feature) {
                    static_cast<void>(standaloneFeatures::placeAt(chunk, random, origin, feature));
                },
                name));
    }
    put(registry, "huge_brown_mushroom", mushroomPlacement(hugeMushroomFeature::Kind::Brown));
    put(registry, "huge_red_mushroom", mushroomPlacement(hugeMushroomFeature::Kind::Red));
    put(registry, "forest_rock",
        [](Chunk& chunk, random::RandomSource& random, std::int32_t x, std::int32_t y, std::int32_t z) {
            forestRockDecoration::placeAt(chunk, random, x, y, z);
        });
    put(registry, "desert_well",
        [](Chunk& chunk, random::RandomSource& random, std::int32_t x, std::int32_t y, std::int32_t z) {
            desertWellDecoration::placeAt(chunk, random, x, y, z);
        });
}

const Registry& registry() {
    static const Registry built = [] {
        Registry map;
        putTrees(map);
        putOresAndSprings(map);
        putPatches(map);
        putFlowersAndVines(map);
        putCavesAndOceans(map);
        putNether(map);
        putStandalone(map);
        return map;
    }();
    return built;
}

} // namespace

const placedFeatures::Placement* placedFeatures::find(const wire::common::Identifier& id) {
    const auto found = registry().find(id);
    return found == registry().end() ? nullptr : &found->second;
}

const std::set<wire::common::Identifier>& placedFeatures::ids() {
    static const std::set<wire::common::Identifier> built = [] {
        std::set<wire::common::Identifier> ids;
        for (const auto& [id, placement] : registry()) {
            ids.insert(id);
        }
        return ids;
    }();
    return built;
}

} // namespace basalt::server::world::terrain::decoration
