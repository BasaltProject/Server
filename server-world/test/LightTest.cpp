#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/light/Light.hpp>

#include <doctest.h>

using namespace basalt::server::world;
using namespace basalt::server::world::light;
using basalt::wire::common::Identifier;
namespace registry = basalt::server::registry;

namespace {

/// A window whose eight neighbours are all missing, which reads as open sky around the middle.
[[nodiscard]] LightWindow lonely(const Chunk& centre) {
    return LightWindow{centre, [](std::int32_t, std::int32_t) -> const Chunk* { return nullptr; }};
}

/// The light of one block of the middle chunk.
[[nodiscard]] std::int32_t at(const std::vector<SectionLight>& light, const Chunk& chunk, std::int32_t x,
                              std::int32_t y, std::int32_t z) {
    const auto section = static_cast<std::size_t>((y >> 4) - chunk.verticalBounds().minSection());
    const std::int32_t localY = y & 15;
    return light[section][static_cast<std::size_t>((localY * 16 + z) * 16 + x)];
}

} // namespace

TEST_CASE("open sky is lit to the top") {
    Chunk chunk{0, 0};
    const std::vector<SectionLight> empty = skyLight::compute(lonely(chunk));
    CHECK(at(empty, chunk, 0, 100, 0) == skyLight::kMax);
    CHECK(at(empty, chunk, 0, -60, 0) == skyLight::kMax);
}

TEST_CASE("a roof over every neighbour too puts what is under it in the dark") {
    // One roofed chunk on its own is not enough: an unloaded neighbour reads as open sky, so light
    // pours in sideways and reaches the middle at fifteen minus the eight blocks it travelled. That
    // is deliberate and documented, and it is why this needs the whole nine roofed.
    std::vector<Chunk> world;
    world.reserve(9);
    for (std::int32_t offsetZ = -1; offsetZ <= 1; ++offsetZ) {
        for (std::int32_t offsetX = -1; offsetX <= 1; ++offsetX) {
            Chunk chunk{offsetX, offsetZ};
            for (std::int32_t x = 0; x < 16; ++x) {
                for (std::int32_t z = 0; z < 16; ++z) {
                    chunk.setBlockStateId(x, 64, z, registry::blocks::stone().stateId);
                }
            }
            world.push_back(std::move(chunk));
        }
    }
    const auto find = [&world](std::int32_t chunkX, std::int32_t chunkZ) -> const Chunk* {
        for (const Chunk& one : world) {
            if (one.chunkX() == chunkX && one.chunkZ() == chunkZ) {
                return &one;
            }
        }
        return nullptr;
    };
    const Chunk& centre = *find(0, 0);
    const LightWindow window{centre, find};
    CHECK(window.isComplete());

    const std::vector<SectionLight> roofed = skyLight::compute(window);
    CHECK(at(roofed, centre, 8, 65, 8) == skyLight::kMax);
    // Under the roof, with roof all around, nothing is left.
    CHECK(at(roofed, centre, 8, 63, 8) == 0);
    CHECK(at(roofed, centre, 0, 63, 0) == 0);
}

TEST_CASE("a slab is not a roof, because it dampens nothing") {
    // parity: getLightDampening comes with the block's shape, so a shape that leaves a gap stops
    // nothing at all. This was the known miss back when everything solid-looking counted as opaque.
    CHECK(skyLight::opacity(registry::blocks::stone().stateId) == 15);
    const std::optional<std::int32_t> slab =
        registry::blocks::stateIdOf(Identifier::vanilla("stone_slab"), {{"type", "bottom"}, {"waterlogged", "false"}});
    REQUIRE(slab.has_value());
    CHECK(skyLight::opacity(*slab) == 0);
    // Water lets light through but takes a level off it.
    CHECK(skyLight::opacity(registry::blocks::water().stateId) == 1);
}

TEST_CASE("nothing burning means no block light at all") {
    Chunk chunk{0, 0};
    CHECK_FALSE(blockLight::compute(lonely(chunk)).has_value());
}

TEST_CASE("a torch lights what is around it and fades with distance") {
    Chunk chunk{0, 0};
    const std::optional<std::int32_t> glowstone = registry::blocks::stateIdOf(Identifier::vanilla("glowstone"), {});
    REQUIRE(glowstone.has_value());
    CHECK(blockLight::emission(*glowstone) == 15);
    CHECK(blockLight::emission(registry::blocks::stone().stateId) == 0);

    chunk.setBlockStateId(8, 64, 8, *glowstone);
    const std::optional<std::vector<SectionLight>> light = blockLight::compute(lonely(chunk));
    REQUIRE(light.has_value());
    CHECK(at(*light, chunk, 8, 64, 8) == 15);
    // One block away through air is one level down, and three blocks is three.
    CHECK(at(*light, chunk, 9, 64, 8) == 14);
    CHECK(at(*light, chunk, 8, 64, 11) == 12);
    // The chunk is only sixteen wide, so the far corner of it is as far as this can check: seven
    // blocks along each axis from the source, which the fill reaches diagonally in fourteen steps.
    CHECK(at(*light, chunk, 15, 64, 15) == 1);
}

TEST_CASE("a window knows whether it had every neighbour") {
    Chunk centre{0, 0};
    CHECK_FALSE(lonely(centre).isComplete());

    Chunk north{0, -1};
    const LightWindow partial{centre, [&north](std::int32_t x, std::int32_t z) -> const Chunk* {
                                  return x == 0 && z == -1 ? &north : nullptr;
                              }};
    CHECK_FALSE(partial.isComplete());
}
