#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "support/DecorationFixture.hpp"

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/decoration/PlacementRegion.hpp>

#include <doctest.h>

#include <cstdint>
#include <optional>
#include <set>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain::decoration;
using basalt::server::world::testsupport::stateIdOf;

/// A step that runs later has to read what an earlier step of the same chunk queued for a cell
/// outside it. Vanilla has no queue: the write lands in the neighbouring proto chunk right away and
/// every later read sees it. Without this an ore read the untouched neighbour, its target test
/// found plain stone, and it overwrote the geode a previous step had put there.
TEST_CASE("a queued external write is visible to a later read") {
    Chunk chunk{0, 0, ChunkHeight{-64, 384}};
    const std::int32_t amethyst = stateIdOf("amethyst_block");
    const std::int32_t deepslate = stateIdOf("deepslate");

    PlacementRegion region{chunk, nullptr, [deepslate](std::int32_t, std::int32_t, std::int32_t) {
                               return std::optional<std::int32_t>{deepslate};
                           }};

    const std::int32_t outsideX = -1;
    CHECK(region.blockStateId(outsideX, -4, 0) == deepslate);

    chunk.setDecorationBlockStateId(outsideX, -4, 0, amethyst, deepslate);
    CHECK(region.blockStateId(outsideX, -4, 0) == amethyst);
}
