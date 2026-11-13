#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/World.hpp>
#include <basalt/server/world/ChunkDataCodec.hpp>
#include <basalt/server/world/light/Light.hpp>
#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/protocol/play/BitStorage.hpp>
#include <basalt/wire/protocol/play/NibbleArray.hpp>
#include <basalt/wire/protocol/play/PalettedContainer.hpp>

#include <algorithm>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <tuple>
#include <unordered_map>

namespace basalt::server::world::chunkDataCodec {
namespace {

namespace play = wire::protocol::play;

constexpr std::size_t kBlocksPerSection = 16 * 16 * 16;
constexpr std::size_t kBiomesPerSection = 4 * 4 * 4;
constexpr std::size_t kPackedSectionBytes = kBlocksPerSection / 2;

[[nodiscard]] int ceilLog2(std::int32_t value) {
    int bits = 0;
    while ((std::int64_t{1} << bits) < value) {
        bits++;
    }
    return bits;
}

/// parity: `Strategy.globalPaletteBitsInMemory`, `ceillog2` of the registry size.
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

using EncodedSection = std::shared_ptr<const std::vector<std::uint8_t>>;

/// Wire bytes of a section keyed by its content version, so every player and resend shares one encoding.
class SectionEncodings {
public:
    static constexpr std::size_t kBudgetBytes = 16UL * 1024UL * 1024UL;

    [[nodiscard]] static SectionEncodings& instance() {
        static SectionEncodings cache;
        return cache;
    }

    [[nodiscard]] EncodedSection find(std::uint64_t version) {
        const std::lock_guard<std::mutex> guard{mutex_};
        const auto found = entries_.find(version);
        return found == entries_.end() ? nullptr : found->second;
    }

    void insert(std::uint64_t version, EncodedSection encoded) {
        const std::lock_guard<std::mutex> guard{mutex_};
        if (!entries_.emplace(version, encoded).second) {
            return;
        }
        order_.push_back(version);
        bytes_ += encoded->size() + kEntryOverhead;
        while (bytes_ > kBudgetBytes && !order_.empty()) {
            const auto oldest = entries_.find(order_.front());
            bytes_ -= oldest->second->size() + kEntryOverhead;
            entries_.erase(oldest);
            order_.pop_front();
        }
    }

private:
    static constexpr std::size_t kEntryOverhead = 96;

    std::mutex mutex_;
    std::unordered_map<std::uint64_t, EncodedSection> entries_;
    std::deque<std::uint64_t> order_;
    std::size_t bytes_ = 0;
};

} // namespace

std::vector<std::uint8_t> encodeSections(const Chunk& chunk) { return encodeSections(chunk, {}); }

std::vector<std::uint8_t> encodeSections(const Chunk& chunk,
                                         const std::vector<std::vector<std::int32_t>>& replacements) {
    wire::io::ByteBuffer buffer;
    std::vector<std::int32_t> blockScratch(kBlocksPerSection);
    std::vector<std::int32_t> biomeScratch(kBiomesPerSection);
    SectionEncodings& cache = SectionEncodings::instance();
    for (std::size_t i = 0; i < chunk.sectionCount(); ++i) {
        const ChunkSection& section = chunk.section(i);
        const bool replaced = i < replacements.size() && !replacements[i].empty();
        const std::uint64_t version = replaced ? 0 : section.contentVersion();
        if (!replaced) {
            if (const EncodedSection hit = cache.find(version); hit != nullptr) {
                buffer.writeBytes(*hit);
                continue;
            }
        }
        const std::size_t start = buffer.writerIndex();
        buffer.writeShort(static_cast<std::int16_t>(section.nonAirBlockCount()));
        // parity: LevelChunkSection.write always writes two shorts, the non-empty block count and then the fluid.
        buffer.writeShort(static_cast<std::int16_t>(section.nonEmptyFluidCount()));
        section.copyBlockStateIdsInto(blockScratch);
        if (replaced) {
            blockScratch = replacements[i];
        }
        play::palettedContainer::write(buffer, blockScratch, play::palettedContainer::kBlockMinIndirectBits,
                                       play::palettedContainer::kBlockMaxIndirectBits, blockDirectBits());
        writeBiomes(buffer, section, biomeScratch);
        if (!replaced) {
            const std::span<const std::uint8_t> written = buffer.readable().subspan(start);
            cache.insert(version, std::make_shared<const std::vector<std::uint8_t>>(written.begin(), written.end()));
        }
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
    std::optional<Chunk::PackedLight> sky;
    if (hasSkylight) {
        sky = chunk.skyLight([&window] { return packAll(light::skyLight::compute(window)); });
    }
    const Chunk::PackedLight block = chunk.blockLight([&window] {
        const std::optional<std::vector<light::SectionLight>> computed = light::blockLight::compute(window);
        return computed.has_value() ? packAll(*computed) : Chunk::PackedLight{};
    });

    LightData data;
    std::tie(data.skyLightMask, data.emptySkyLightMask, data.skyLight) = masked(sky.has_value() ? &*sky : nullptr);
    std::tie(data.blockLightMask, data.emptyBlockLightMask, data.blockLight) = masked(&block);
    return data;
}

LightData encodeLight(Chunk& chunk, bool hasSkylight) {
    const light::LightWindow window{chunk, [](std::int32_t, std::int32_t) -> const Chunk* { return nullptr; }};
    return encodeLight(chunk, hasSkylight, window);
}

} // namespace basalt::server::world::chunkDataCodec
