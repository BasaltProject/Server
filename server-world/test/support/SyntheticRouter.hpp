#pragma once

#include <basalt/server/world/terrain/NoiseRouterData.hpp>

#include <doctest.h>

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace basalt::server::world::testsupport {

/// A `minecraft:noise` node over one of the resolve-anything noises.
[[nodiscard]] inline std::string noiseNode(std::string_view noise, double xzScale, double yScale) {
    return R"({"type": "minecraft:noise", "noise": ")" + std::string{noise} + R"(", "xz_scale": )" +
           std::to_string(xzScale) + R"(, "y_scale": )" + std::to_string(yScale) + "}";
}

/// A rolling floor: a y gradient from `low` to `high` plus a doubled `minecraft:test` noise.
[[nodiscard]] inline std::string rollingFloor(std::int32_t low, std::int32_t high, double xzScale, double yScale) {
    return R"({"type": "minecraft:add", "left": {"type": "minecraft:gradient", "axis": "y", "from_coordinate": )" +
           std::to_string(low) + R"(, "to_coordinate": )" + std::to_string(high) +
           R"(, "from_value": 1.0, "to_value": -1.0}, "right": {"type": "minecraft:mul", "left": 2.0, "right": )" +
           noiseNode("minecraft:test", xzScale, yScale) + "}}";
}

/// Six climate axes over independent noises, with depth falling through y, so a biome search has a
/// whole parameter space to walk.
[[nodiscard]] inline std::map<std::string, std::string> climateFields() {
    return {{"temperature", noiseNode("minecraft:temperature", 0.25, 0.0)},
            {"vegetation", noiseNode("minecraft:vegetation", 0.25, 0.0)},
            {"continents", noiseNode("minecraft:continentalness", 0.25, 0.0)},
            {"erosion", noiseNode("minecraft:erosion", 0.25, 0.0)},
            {"depth", R"({"type": "minecraft:gradient", "axis": "y", "from_coordinate": -64, "to_coordinate": )"
                      R"(320, "from_value": 1.5, "to_value": -1.5})"},
            {"ridges", noiseNode("minecraft:ridge", 0.25, 0.0)}};
}

/// A dimension recipe written for the test, loaded through the same path as the jar's: every noise
/// resolves to one small octave stack, the router answers zero except `final_density` and `fields`,
/// and there is no aquifer, surface or carver. Nothing here is Mojang data.
struct Shape {
    std::int32_t minY = -64;
    std::int32_t height = 384;
    std::int32_t seaLevel = 63;
    std::string defaultBlock = "minecraft:stone";
    std::string defaultFluid = "minecraft:water";
    std::map<std::string, std::string> fields;
};

[[nodiscard]] inline terrain::NoiseRouter syntheticRouter(std::int64_t seed, std::string_view finalDensity,
                                                          const Shape& shape = {}) {
    const std::map<std::string, std::string>& fields = shape.fields;
    std::string settings = R"({"sea_level": )" + std::to_string(shape.seaLevel) + R"(, "default_block": ")" +
                           shape.defaultBlock + R"(", "default_fluid": ")" + shape.defaultFluid +
                           R"(", "noise": {"min_y": )" + std::to_string(shape.minY) + R"(, "height": )" +
                           std::to_string(shape.height) + R"(}, "noise_router": {)";
    for (const std::string_view field :
         {"temperature", "vegetation", "continents", "erosion", "depth", "ridges", "chunk_surface_level"}) {
        const auto found = fields.find(std::string{field});
        settings += "\"" + std::string{field} + "\": " + (found == fields.end() ? "0.0" : found->second) + ", ";
    }
    settings += R"("final_density": )" + std::string{finalDensity} +
                R"(}, "material_rule": {"type": "minecraft:block", "result_state": ")" + shape.defaultBlock + "\"}}";
    const densityfunction::WorldgenReader reader = [settings](std::string_view path) -> std::optional<std::string> {
        if (path == "minecraft/worldgen/noise_settings/test") {
            return settings;
        }
        if (path.starts_with("minecraft/worldgen/biome/")) {
            return std::string{R"({"carvers": []})"};
        }
        if (path.starts_with("minecraft/worldgen/noise/")) {
            return std::string{R"({"base_octave": -6, "octave_count": 2})"};
        }
        return std::nullopt;
    };
    auto router = terrain::noiseRouterData::load("minecraft:test", seed, reader);
    if (!router.has_value()) {
        FAIL(router.error());
    }
    return std::move(*router);
}

/// An End-shaped recipe: the outer islands as erosion and a floor that thins out with height.
[[nodiscard]] inline terrain::NoiseRouter endRouter(std::int64_t seed) {
    const std::string islands = R"({"type": "minecraft:end_outer_islands"})";
    const std::string density =
        R"({"type": "minecraft:add", "left": )" + islands +
        R"(, "right": {"type": "minecraft:gradient", "axis": "y", "from_coordinate": 40, "to_coordinate": 80, )"
        R"("from_value": 0.5, "to_value": -0.5}})";
    return syntheticRouter(
        seed, density,
        {.minY = 0, .height = 128, .defaultBlock = "minecraft:end_stone", .fields = {{"erosion", islands}}});
}
} // namespace basalt::server::world::testsupport
