#include <basalt/server/world/structure/StructureSpawnOverrides.hpp>

#include <cstdint>
#include <map>

namespace basalt::server::world::structure {
namespace {

using structureSpawnOverrides::Bounds;
using structureSpawnOverrides::Override;
using structureSpawnOverrides::SpawnEntry;

/// A uniform `count` range.
[[nodiscard]] SpawnEntry entry(std::string_view type, std::int32_t weight, std::int32_t min, std::int32_t max) {
    return SpawnEntry{.type = "minecraft:" + std::string{type}, .weight = weight, .minCount = min, .maxCount = max};
}

/// A constant `count`.
[[nodiscard]] SpawnEntry entry(std::string_view type, std::int32_t weight, std::int32_t count) {
    return SpawnEntry{.type = "minecraft:" + std::string{type},
                      .weight = weight,
                      .minCount = count,
                      .maxCount = count,
                      .constantCount = true};
}

[[nodiscard]] Override emptyFull() { return Override{.bounds = Bounds::Full, .spawns = {}}; }

using CategoryMap = std::map<std::string, Override, std::less<>>;

const std::map<std::string, CategoryMap, std::less<>>& byStructure() {
    static const std::map<std::string, CategoryMap, std::less<>> built = [] {
        std::map<std::string, CategoryMap, std::less<>> map;
        map["fortress"]["monster"] =
            Override{.bounds = Bounds::Piece,
                     .spawns = {entry("blaze", 10, 2, 3), entry("zombified_piglin", 5, 4),
                                entry("wither_skeleton", 8, 5), entry("skeleton", 2, 5), entry("magma_cube", 3, 4)}};
        map["monument"]["monster"] = Override{.bounds = Bounds::Full, .spawns = {entry("guardian", 1, 2, 4)}};
        map["monument"]["axolotls"] = emptyFull();
        map["monument"]["underground_water_creature"] = emptyFull();
        map["pillager_outpost"]["monster"] = Override{.bounds = Bounds::Full, .spawns = {entry("pillager", 1, 1)}};
        map["swamp_hut"]["monster"] = Override{.bounds = Bounds::Piece, .spawns = {entry("witch", 1, 1)}};
        map["swamp_hut"]["creature"] = Override{.bounds = Bounds::Piece, .spawns = {entry("cat", 1, 1)}};
        for (const std::string_view category :
             {"monster", "creature", "ambient", "axolotls", "underground_water_creature", "water_creature",
              "water_ambient", "misc"}) {
            map["ancient_city"][std::string{category}] = emptyFull();
            map["trial_chambers"][std::string{category}] = Override{.bounds = Bounds::Piece, .spawns = {}};
        }
        return map;
    }();
    return built;
}

} // namespace

const std::set<std::string, std::less<>>& structureSpawnOverrides::structures() {
    static const std::set<std::string, std::less<>> names = [] {
        std::set<std::string, std::less<>> built;
        for (const auto& [structure, categories] : byStructure()) {
            built.insert(structure);
        }
        return built;
    }();
    return names;
}

const Override* structureSpawnOverrides::of(const std::string_view structure, const std::string_view category) {
    const auto found = byStructure().find(structure);
    if (found == byStructure().end()) {
        return nullptr;
    }
    const auto entry = found->second.find(category);
    return entry == found->second.end() ? nullptr : &entry->second;
}

} // namespace basalt::server::world::structure
