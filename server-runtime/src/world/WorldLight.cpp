#include <basalt/server/runtime/world/WorldLight.hpp>
#include <basalt/server/world/light/Light.hpp>
#include <basalt/server/world/light/LightWindow.hpp>
#include <basalt/wire/protocol/play/NibbleArray.hpp>

#include <algorithm>
#include <cstdint>

namespace basalt::server::runtime::world::worldLight {
namespace {

using server::world::Chunk;
using server::world::light::LightWindow;
using server::world::light::SectionLight;

[[nodiscard]] Chunk::PackedLight packAll(const std::vector<SectionLight>& sections) {
    Chunk::PackedLight packed;
    packed.reserve(sections.size());
    for (const SectionLight& section : sections) {
        std::vector<std::uint8_t> bytes(section.size() / 2);
        wire::protocol::play::nibbleArray::pack(section, bytes);
        packed.push_back(std::move(bytes));
    }
    return packed;
}

[[nodiscard]] std::int32_t level(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, bool sky,
                                 bool hasSkylight) {
    Chunk* chunk = world.chunkIfLoaded(x >> 4, z >> 4);
    if (chunk == nullptr) {
        return 0;
    }
    if (y < chunk->verticalBounds().minY() || y > chunk->verticalBounds().maxY()) {
        return 0;
    }
    if (sky && !hasSkylight) {
        return 0;
    }

    const Chunk::PackedLight* cached = sky ? chunk->cachedSkyLight() : chunk->cachedBlockLight();
    Chunk::PackedLight computedHere;
    if (cached == nullptr) {
        const LightWindow::Neighbours neighbours = [&world](std::int32_t chunkX, std::int32_t chunkZ) {
            return world.chunkIfLoaded(chunkX, chunkZ);
        };
        if (sky) {
            computedHere = chunk->skyLight([chunk, &neighbours] {
                return packAll(server::world::light::skyLight::compute(LightWindow{*chunk, neighbours}));
            });
        } else {
            computedHere = chunk->blockLight([chunk, &neighbours] {
                const std::optional<std::vector<SectionLight>> computed =
                    server::world::light::blockLight::compute(LightWindow{*chunk, neighbours});
                return computed.has_value() ? packAll(*computed) : Chunk::PackedLight{};
            });
        }
        cached = &computedHere;
    }

    const std::size_t section = static_cast<std::size_t>((y - chunk->verticalBounds().minY()) >> 4);
    if (section >= cached->size()) {
        return 0;
    }
    const std::vector<std::uint8_t>& bytes = (*cached)[section];
    if (bytes.empty()) {
        return 0;
    }
    const std::size_t index = static_cast<std::size_t>(((y & 15) << 8) | ((z & 15) << 4) | (x & 15));
    const std::uint8_t byte = bytes[index / 2];
    return index % 2 == 0 ? byte & 0xF : (byte >> 4) & 0xF;
}

} // namespace

std::int32_t skyLight(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, bool hasSkylight) {
    return level(world, x, y, z, true, hasSkylight);
}

std::int32_t blockLight(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    return level(world, x, y, z, false, true);
}

std::int32_t rawBrightness(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, bool hasSkylight,
                           std::int32_t skyDampen) {
    return std::max(level(world, x, y, z, false, hasSkylight), level(world, x, y, z, true, hasSkylight) - skyDampen);
}

} // namespace basalt::server::runtime::world::worldLight
