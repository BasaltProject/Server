#include <basalt/server/registry/World.hpp>
#include <basalt/server/runtime/bedrock/BedrockBlockEntities.hpp>
#include <basalt/server/runtime/bedrock/BedrockChunks.hpp>
#include <basalt/server/runtime/world/Signs.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/ChunkSection.hpp>
#include <basalt/wire/bedrock/BedrockBanners.hpp>
#include <basalt/wire/bedrock/BedrockBiomes.hpp>
#include <basalt/wire/bedrock/BedrockColumn.hpp>
#include <basalt/wire/bedrock/BedrockJavaBlockEntities.hpp>
#include <basalt/wire/bedrock/BedrockPackets.hpp>
#include <basalt/wire/io/ByteBuffer.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::bedrock {
namespace {

[[nodiscard]] std::int32_t bedrockBiomeOf(const std::int32_t javaId) {
    static const std::vector<std::int32_t> kByJavaId = [] {
        const auto& registry = server::registry::biomes::registry();
        std::vector<std::int32_t> built(registry.size(), wire::bedrock::kFallbackBedrockBiome);
        for (std::size_t id = 0; id < built.size(); ++id) {
            if (const server::registry::Biome* biome = registry.byId(static_cast<std::int32_t>(id)); biome != nullptr) {
                built[id] =
                    wire::bedrock::bedrockBiomeId(biome->id.toString()).value_or(wire::bedrock::kFallbackBedrockBiome);
            }
        }
        return built;
    }();
    return javaId >= 0 && static_cast<std::size_t>(javaId) < kByJavaId.size()
               ? kByJavaId[static_cast<std::size_t>(javaId)]
               : wire::bedrock::kFallbackBedrockBiome;
}

} // namespace

std::vector<std::uint8_t> writeColumn(const server::world::Chunk& chunk, const int protocol,
                                      const std::vector<std::vector<std::int32_t>>& replacements) {
    const auto sections = static_cast<std::int32_t>(chunk.sectionCount());
    const std::int32_t bottom = chunk.verticalBounds().minY();

    // Only up to the highest section with content: all-air subchunks cost five hundred bytes each in every column.
    std::int32_t sent = 0;
    for (std::int32_t section = sections - 1; section >= 0; --section) {
        if (chunk.section(static_cast<std::size_t>(section)).nonAirBlockCount() > 0) {
            sent = section + 1;
            break;
        }
    }

    // Collected while reading blocks: that edition keeps a bed's colour in a block entity, so no second scan.
    struct Bed {
        std::int32_t x = 0;
        std::int32_t y = 0;
        std::int32_t z = 0;
        std::uint8_t colour = 0;
    };

    std::vector<Bed> beds;

    // Beds are noted while the blocks are read, so no second scan finds them.
    const wire::bedrock::FillBlocks fillBlocks = [&](const std::int32_t section, std::span<std::int32_t> blocks) {
        for (std::int32_t y = 0; y < 16; ++y) {
            for (std::int32_t z = 0; z < 16; ++z) {
                for (std::int32_t x = 0; x < 16; ++x) {
                    const std::size_t at = (static_cast<std::size_t>(y) << 8U) | (static_cast<std::size_t>(z) << 4U) |
                                           static_cast<std::size_t>(x);
                    const auto index = static_cast<std::size_t>(section);
                    blocks[at] = index < replacements.size() && !replacements[index].empty()
                                     ? replacements[index][at]
                                     : chunk.blockStateId(x, bottom + (section * 16) + y, z);
                    if (const auto colour = bedColourOf(blocks[at]); colour) {
                        beds.push_back({x, bottom + (section * 16) + y, z, *colour});
                    }
                }
            }
        }
    };
    // parity: Geyser's `BiomeTranslator`.
    const wire::bedrock::FillBiomes fillBiomes = [&](const std::int32_t section, wire::bedrock::BiomeQuarts& quarts) {
        std::array<std::int32_t, server::world::ChunkSection::kBiomes> javaIds{};
        chunk.section(static_cast<std::size_t>(section)).copyBiomeIdsInto(javaIds);
        for (std::size_t quart = 0; quart < javaIds.size(); ++quart) {
            quarts[quart] = bedrockBiomeOf(javaIds[quart]);
        }
    };

    const wire::bedrock::FillBlockEntities fillTags = [&](wire::io::ByteBuffer& extras) {
        for (const Bed& bed : beds) {
            wire::bedrock::writeBedTag(extras, (chunk.chunkX() * 16) + bed.x, bed.y, (chunk.chunkZ() * 16) + bed.z,
                                       bed.colour);
        }
        for (const server::world::ChunkBlockEntity& entity : chunk.blockEntities()) {
            const std::int32_t at = chunk.blockStateId(entity.localX(), entity.y(), entity.localZ());
            const std::int32_t type = bannerColourOf(at)         ? wire::bedrock::kJavaBannerType
                                      : world::signs::isSign(at) ? wire::bedrock::kJavaSignType
                                                                 : -1;
            wire::bedrock::writeJavaBlockEntity(extras, (chunk.chunkX() * 16) + entity.localX(), entity.y(),
                                                (chunk.chunkZ() * 16) + entity.localZ(), type, entity.data(), at);
        }
    };

    wire::bedrock::ColumnShape shape;
    shape.chunkX = chunk.chunkX();
    shape.chunkZ = chunk.chunkZ();
    shape.minSection = chunk.verticalBounds().minSection();
    shape.sections = sections;
    shape.sent = sent;
    shape.protocol = protocol;
    // The subchunk count is what was written; the biome section count is the column's own height.
    return wire::bedrock::encodeColumn(shape, fillBlocks, fillBiomes, fillTags);
}

} // namespace basalt::server::runtime::bedrock
