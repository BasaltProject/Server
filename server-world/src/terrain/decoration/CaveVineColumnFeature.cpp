#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/CaveVineColumnFeature.hpp>

#include <algorithm>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>

namespace basalt::server::world::terrain::decoration {
namespace {

[[nodiscard]] std::int32_t state(std::string_view path, const std::map<std::string, std::string>& properties) {
    const std::optional<std::int32_t> stateId =
        registry::blocks::stateIdOf(wire::common::Identifier::vanilla(std::string{path}), properties);
    if (!stateId.has_value()) {
        throw std::runtime_error{"unknown block state '" + std::string{path} + "'"};
    }
    return *stateId;
}

/// parity: the configured feature's height provider, a weighted list rather than a uniform draw, so
/// which branch runs decides how many further values are consumed.
[[nodiscard]] std::int32_t sampleCaveHeight(random::RandomSource& random) {
    const std::int32_t choice = random.nextInt(15);
    if (choice <= 1) {
        return random.nextInt(20);
    }
    if (choice <= 4) {
        return random.nextInt(3);
    }
    return random.nextInt(7);
}

[[nodiscard]] std::int32_t sampleMossHeight(random::RandomSource& random) {
    return random.nextInt(6) < 5 ? random.nextInt(4) : 1 + random.nextInt(7);
}

[[nodiscard]] std::int32_t caveVinePlantState(random::RandomSource& random) {
    return random.nextInt(5) < 4 ? state("cave_vines_plant", {{"berries", "false"}})
                                 : state("cave_vines_plant", {{"berries", "true"}});
}

[[nodiscard]] std::int32_t caveVineTipState(random::RandomSource& random) {
    const bool berries = random.nextInt(5) >= 4;
    const std::int32_t age = 23 + random.nextInt(3);
    return state("cave_vines", {{"age", std::to_string(age)}, {"berries", berries ? "true" : "false"}});
}

} // namespace

bool caveVineColumnFeature::place(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin,
                                  bool inMoss) {
    std::int32_t bodyHeight = inMoss ? sampleMossHeight(random) : sampleCaveHeight(random);
    std::int32_t tipHeight = 1;
    const std::int32_t totalHeight = bodyHeight + tipHeight;

    const ChunkHeight bounds = chunk.verticalBounds();
    std::int32_t validHeight = totalHeight;
    DecorationPos next{origin.x, origin.y - 1, origin.z};
    for (std::int32_t distance = 0; distance < totalHeight; ++distance) {
        if (next.y < bounds.minY() || next.y > bounds.maxY() ||
            !registry::blocks::isAir(chunk.decorationBlockStateId(next.x, next.y, next.z))) {
            validHeight = distance;
            break;
        }
        --next.y;
    }

    // parity: `prioritize_tip` removes body blocks first and keeps the final tip.
    std::int32_t toRemove = totalHeight - validHeight;
    const std::int32_t bodyRemoved = std::min(bodyHeight, toRemove);
    bodyHeight -= bodyRemoved;
    toRemove -= bodyRemoved;
    tipHeight -= std::min(tipHeight, toRemove);

    DecorationPos position = origin;
    for (std::int32_t placed = 0; placed < bodyHeight; ++placed) {
        chunk.setDecorationBlockStateId(position.x, position.y, position.z, caveVinePlantState(random));
        --position.y;
    }
    for (std::int32_t placed = 0; placed < tipHeight; ++placed) {
        chunk.setDecorationBlockStateId(position.x, position.y, position.z, caveVineTipState(random));
        --position.y;
    }
    return bodyHeight + tipHeight > 0;
}

} // namespace basalt::server::world::terrain::decoration
