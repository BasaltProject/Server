#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/end/EndFeatureBlocks.hpp>

#include <cmath>
#include <cstdlib>

namespace basalt::server::world::terrain::decoration::end {

void endGatewayFeature::placeBlocks(std::int32_t x, std::int32_t y, std::int32_t z, const endSpikes::SetBlock& set) {
    const std::int32_t air = registry::blocks::air().stateId;
    const std::int32_t bedrock = registry::blocks::bedrock().stateId;
    const registry::BlockState* gateway = registry::blocks::named("end_gateway");
    for (std::int32_t pz = z - 1; pz <= z + 1; ++pz) {
        for (std::int32_t py = y - 2; py <= y + 2; ++py) {
            for (std::int32_t px = x - 1; px <= x + 1; ++px) {
                const bool sameX = px == x;
                const bool sameY = py == y;
                const bool sameZ = pz == z;
                const bool end = std::abs(py - y) == 2;
                std::int32_t stateId = air;
                if (sameX && sameY && sameZ) {
                    stateId = gateway == nullptr ? air : gateway->stateId;
                } else if (sameY) {
                    stateId = air;
                } else if ((end && sameX && sameZ) || ((sameX || sameZ) && !end)) {
                    stateId = bedrock;
                }
                set(px, py, pz, stateId);
            }
        }
    }
}

void endIslandFeature::placeBlocks(random::RandomSource& random, std::int32_t x, std::int32_t y, std::int32_t z,
                                   const endSpikes::SetBlock& set) {
    const std::int32_t endStone = registry::blocks::named("end_stone")->stateId;
    auto size = static_cast<float>(random.nextInt(3)) + 4.0F;
    for (std::int32_t dy = 0; size > 0.5F; --dy) {
        const auto from = static_cast<std::int32_t>(std::floor(-size));
        const auto to = static_cast<std::int32_t>(std::ceil(size));
        for (std::int32_t dx = from; dx <= to; ++dx) {
            for (std::int32_t dz = from; dz <= to; ++dz) {
                if (static_cast<float>((dx * dx) + (dz * dz)) <= (size + 1.0F) * (size + 1.0F)) {
                    set(x + dx, y + dy, z + dz, endStone);
                }
            }
        }
        size -= static_cast<float>(random.nextInt(2)) + 0.5F;
    }
}

} // namespace basalt::server::world::terrain::decoration::end
