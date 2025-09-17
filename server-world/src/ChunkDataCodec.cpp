#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/World.hpp>
#include <basalt/server/world/ChunkDataCodec.hpp>
#include <basalt/server/world/light/Light.hpp>
#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/protocol/play/BitStorage.hpp>
#include <basalt/wire/protocol/play/NibbleArray.hpp>
#include <basalt/wire/protocol/play/PalettedContainer.hpp>

#include <algorithm>
#include <tuple>

namespace basalt::server::world::chunkDataCodec {
namespace {

namespace play = wire::protocol::play;

constexpr std::size_t kBlocksPerSection = 16 * 16 * 16;
constexpr std::size_t kBiomesPerSection = 4 * 4 * 4;
/// One nibble per block: half a byte each, 4096 to a section.
constexpr std::size_t kPackedSectionBytes = kBlocksPerSection / 2;

[[nodiscard]] int ceilLog2(std::int32_t value) {
    int bits = 0;
    while ((std::int64_t{1} << bits) < value) {
        bits++;
    }
    return bits;
}

/// parity: `Strategy.globalPaletteBitsInMemory`, `ceillog2` of the registry size.
///
/// A flat 8 and 4 here truncated the ids of any section past an indirect palette, and the client
/// reads eight bits as a palette, so it lost the rest of the chunk with them.
[[nodiscard]] int blockDirectBits() {
    static const int bits = ceilLog2(registry::blocks::stateCount());
    return bits;
}

[[nodiscard]] int biomeDirectBits() {
    static const int bits = ceilLog2(static_cast<std::int32_t>(registry::biomes::registry().size()));
    return bits;
}

void writeBiomes(wire::io::ByteBuffer& buffer, const ChunkSection& section, std::vector<std::int32_t>& scratch) {
    section.copyBiomeIdsInto(scratch);
    play::palettedContainer::write(buffer, scratch, play::palettedContainer::kBiomeMinIndirectBits,
                                   play::palettedContainer::kBiomeMaxIndirectBits, biomeDirectBits());
}

/// Splits packed sections into the two masks and the arrays the packet carries.
[[nodiscard]] std::tuple<std::vector<std::int64_t>, std::vector<std::int64_t>, std::vector<std::vector<std::uint8_t>>>
masked(const Chunk::PackedLight* sections) {
    if (sections == nullptr || sections->empty()) {
        return {{0}, {0}, {}};
    }
    std::int64_t maskBits = 0;
    std::int64_t emptyMaskBits = 0;
    std::vector<std::vector<std::uint8_t>> arrays;
    for (std::size_t i = 0; i < sections->size(); ++i) {
        const std::vector<std::uint8_t>& packed = (*sections)[i];
        if (std::ranges::all_of(packed, [](std::uint8_t byte) { return byte == 0; })) {
            emptyMaskBits |= std::int64_t{1} << i;
        } else {
            maskBits |= std::int64_t{1} << i;
            arrays.push_back(packed);
        }
    }
    return {{maskBits}, {emptyMaskBits}, std::move(arrays)};
}

[[nodiscard]] Chunk::PackedLight packAll(const std::vector<light::SectionLight>& computed) {
    Chunk::PackedLight packed;
    packed.reserve(computed.size());
    for (const light::SectionLight& section : computed) {
        std::vector<std::uint8_t> bytes(kPackedSectionBytes);
        play::nibbleArray::pack(section, bytes);
        packed.push_back(std::move(bytes));
    }
    return packed;
}

} // namespace

std::vector<std::uint8_t> encodeSections(const Chunk& chunk) {
    wire::io::ByteBuffer buffer;
    // One scratch per expansion, shared across all sections: expanding each section into its own
    // fresh array put roughly 400 KB of garbage behind every chunk sent.
    std::vector<std::int32_t> blockScratch(kBlocksPerSection);
    std::vector<std::int32_t> biomeScratch(kBiomesPerSection);
    for (std::size_t i = 0; i < chunk.sectionCount(); ++i) {
        const ChunkSection& section = chunk.section(i);
        buffer.writeShort(static_cast<std::int16_t>(section.nonAirBlockCount()));
        // parity: LevelChunkSection.write always writes two shorts, the non-empty block count and
        // then the fluid count, never just one: a real client overran the buffer reading the next
        // section's data when this field was missing.
        buffer.writeShort(static_cast<std::int16_t>(section.nonEmptyFluidCount()));
        section.copyBlockStateIdsInto(blockScratch);
        play::palettedContainer::write(buffer, blockScratch, play::palettedContainer::kBlockMinIndirectBits,
                                       play::palettedContainer::kBlockMaxIndirectBits, blockDirectBits());
        writeBiomes(buffer, section, biomeScratch);
    }
    return buffer.toVector();
}

std::vector<std::uint8_t> encodeBiomes(const Chunk& chunk) {
    wire::io::ByteBuffer buffer;
    std::vector<std::int32_t> biomeScratch(kBiomesPerSection);
    for (std::size_t i = 0; i < chunk.sectionCount(); ++i) {
        writeBiomes(buffer, chunk.section(i), biomeScratch);
    }
    return buffer.toVector();
}

std::vector<std::int64_t> encodeHeightmap(const Chunk& chunk) {
    const int bitsPerEntry = ceilLog2(chunk.verticalBounds().height() + 1);
    play::BitStorage storage{bitsPerEntry, 256};
    const std::vector<std::int32_t> values = chunk.heightmap();
    for (std::size_t i = 0; i < values.size(); ++i) {
        storage.set(i, values[i] - chunk.verticalBounds().minY());
    }
    std::vector<std::int64_t> words;
    words.reserve(storage.words().size());
    for (const std::uint64_t word : storage.words()) {
        words.push_back(static_cast<std::int64_t>(word));
    }
    return words;
}

LightData encodeLight(Chunk& chunk, bool hasSkylight, const light::LightWindow& window) {
    const Chunk::PackedLight* sky = nullptr;
    if (hasSkylight) {
        sky = &chunk.skyLight([&window] { return packAll(light::skyLight::compute(window)); });
    }
    const Chunk::PackedLight& block = chunk.blockLight([&window] {
        const std::optional<std::vector<light::SectionLight>> computed = light::blockLight::compute(window);
        return computed.has_value() ? packAll(*computed) : Chunk::PackedLight{};
    });

    LightData data;
    std::tie(data.skyLightMask, data.emptySkyLightMask, data.skyLight) = masked(sky);
    std::tie(data.blockLightMask, data.emptyBlockLightMask, data.blockLight) = masked(&block);
    return data;
}

LightData encodeLight(Chunk& chunk, bool hasSkylight) {
    const light::LightWindow window{chunk, [](std::int32_t, std::int32_t) -> const Chunk* { return nullptr; }};
    return encodeLight(chunk, hasSkylight, window);
}

} // namespace basalt::server::world::chunkDataCodec
