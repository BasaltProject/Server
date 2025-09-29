#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/JungleGrassDecoration.hpp>
#include <basalt/server/world/terrain/decoration/SimpleBlockFeature.hpp>

#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::server::world::terrain::decoration {
namespace {

[[nodiscard]] std::int32_t stateIdOf(std::string_view path) {
    const registry::BlockState* found = registry::blocks::named(path);
    if (found == nullptr) {
        throw std::runtime_error{"unknown block '" + std::string{path} + "'"};
    }
    return found->stateId;
}

/// parity: the placed feature's own `block_predicate_filter`, air whose support is not podzol.
[[nodiscard]] PlacementModifier airNotOnPodzol() {
    return filter([](const PlacementContext& context, random::RandomSource&, const DecorationPos& position) {
        const Chunk& chunk = context.chunk();
        const ChunkHeight bounds = chunk.verticalBounds();
        if (position.y <= bounds.minY() || position.y > bounds.maxY()) {
            return false;
        }
        if (!registry::blocks::isAir(chunk.decorationBlockStateId(position.x, position.y, position.z))) {
            return false;
        }
        const wire::common::Identifier* below = registry::blockStateDefinitions::blockOf(
            chunk.decorationBlockStateId(position.x, position.y - 1, position.z));
        return below == nullptr || below->path() != "podzol";
    });
}

} // namespace

void jungleGrassDecoration::placePatchAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin,
                                         const PlacementContext&) {
    const SimpleBlockFeature feature{[](random::RandomSource& r, const DecorationPos&) {
        return r.nextInt(4) < 3 ? stateIdOf("short_grass") : stateIdOf("fern");
    }};
    const std::vector<PlacementModifier> modifiers{countPlacement(32), randomOffsetPlacement(7, 3), airNotOnPodzol()};
    placeFrom(chunk, random, origin, modifiers,
              [&](const DecorationPos& position) { feature.place(chunk, random, position); });
}

void jungleGrassDecoration::decorate(Chunk& chunk, random::RandomSource& random) {
    const PlacementContext context{chunk};
    const DecorationPos origin{.x = chunk.chunkX() * 16, .y = 0, .z = chunk.chunkZ() * 16};
    const std::vector<PlacementModifier> modifiers{countPlacement(25), inSquarePlacement(), heightmapPlacement()};
    placeFrom(chunk, random, origin, modifiers,
              [&](const DecorationPos& position) { placePatchAt(chunk, random, position, context); });
}

} // namespace basalt::server::world::terrain::decoration
