#include <basalt/server/registry/World.hpp>
#include <basalt/server/world/terrain/decoration/BiomePresence.hpp>

#include <expected>
#include <string>

namespace basalt::server::world::terrain::decoration {

std::optional<bool> biomeInStoredRange(const Chunk& chunk, const std::string_view biome,
                                       const ExternalBiomeReader& externalBiome) {
    const std::expected<wire::common::Identifier, wire::common::IdentifierError> id =
        wire::common::Identifier::parse("minecraft:" + std::string{biome});
    const std::optional<std::int32_t> biomeId = id.has_value() ? registry::biomes::registry().idOf(*id) : std::nullopt;
    if (!biomeId.has_value()) {
        return std::nullopt;
    }
    if (chunk.hasBiome(*biomeId)) {
        return true;
    }
    if (!externalBiome) {
        return std::nullopt;
    }
    for (std::int32_t dx = -1; dx <= 1; ++dx) {
        for (std::int32_t dz = -1; dz <= 1; ++dz) {
            if (dx == 0 && dz == 0) {
                continue;
            }
            const std::optional<bool> stored = externalBiome(chunk.chunkX() + dx, chunk.chunkZ() + dz, *biomeId);
            if (!stored.has_value()) {
                return std::nullopt;
            }
            if (*stored) {
                return true;
            }
        }
    }
    return false;
}

} // namespace basalt::server::world::terrain::decoration
