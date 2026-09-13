#include <basalt/server/runtime/bedrock/BedrockChunks.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/ChunkSection.hpp>
#include <basalt/wire/bedrock/BedrockPackets.hpp>
#include <basalt/wire/io/ByteBuffer.hpp>

#include <array>
#include <cstdint>

namespace basalt::server::runtime::bedrock {

std::vector<std::uint8_t> writeColumn(const server::world::Chunk& chunk, const int protocol) {
    const auto sections = static_cast<std::int32_t>(chunk.sectionCount());
    const std::int32_t bottom = chunk.verticalBounds().minY();

    // Only up to the highest section with anything in it. A subchunk of pure air still costs its
    // five hundred bytes of indices, and a world that is empty above the treeline was paying for
    // fourteen of them in every column: two thirds of a join was air.
    std::int32_t sent = 0;
    for (std::int32_t section = sections - 1; section >= 0; --section) {
        if (chunk.section(static_cast<std::size_t>(section)).nonAirBlockCount() > 0) {
            sent = section + 1;
            break;
        }
    }

    wire::io::ByteBuffer packed{static_cast<std::size_t>(sent) * 1024};
    std::array<std::int32_t, wire::bedrock::kSubChunkBlocks> blocks{};
    for (std::int32_t section = 0; section < sent; ++section) {
        for (std::int32_t y = 0; y < 16; ++y) {
            for (std::int32_t z = 0; z < 16; ++z) {
                for (std::int32_t x = 0; x < 16; ++x) {
                    const std::size_t at = (static_cast<std::size_t>(y) << 8U) | (static_cast<std::size_t>(z) << 4U) |
                                           static_cast<std::size_t>(x);
                    blocks[at] = chunk.blockStateId(x, bottom + (section * 16) + y, z);
                }
            }
        }
        // Bedrock counts its subchunks from the bottom of the world, which for the overworld starts
        // four below zero: the index is the section's own place, not its distance from y zero.
        wire::bedrock::writeSubChunk(packed, blocks,
                                     static_cast<std::int8_t>(chunk.verticalBounds().minSection() + section), protocol);
    }

    wire::io::ByteBuffer body;
    // The count of subchunks is what was written; the count of biome sections is the column's own
    // height, which does not shrink with them.
    wire::bedrock::writeLevelChunk(body, chunk.chunkX(), chunk.chunkZ(), static_cast<std::uint32_t>(sent),
                                   packed.readable(), static_cast<std::uint32_t>(sections), 0, protocol);
    return body.toVector();
}

} // namespace basalt::server::runtime::bedrock
