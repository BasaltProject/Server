#include <basalt/server/runtime/bedrock/BedrockBlockEntities.hpp>
#include <basalt/server/runtime/bedrock/BedrockChunks.hpp>
#include <basalt/server/runtime/world/Signs.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/ChunkSection.hpp>
#include <basalt/wire/bedrock/BedrockBanners.hpp>
#include <basalt/wire/bedrock/BedrockPackets.hpp>
#include <basalt/wire/io/ByteBuffer.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::bedrock {
namespace {

/// parity: Geyser's own eight, which it looks for rather than translating: that edition draws the
/// layers of an ominous banner wrong and has a drawing of its own for the whole thing.
[[nodiscard]] bool isOminous(const wire::nbt::NbtList& layers) {
    static constexpr std::array<std::pair<std::string_view, std::string_view>, 8> kOminous{{
        {"rhombus", "cyan"},
        {"stripe_bottom", "light_gray"},
        {"stripe_center", "gray"},
        {"border", "light_gray"},
        {"stripe_middle", "black"},
        {"half_horizontal", "light_gray"},
        {"circle", "light_gray"},
        {"border", "black"},
    }};

    if (layers.size() != kOminous.size()) {
        return false;
    }
    for (std::size_t at = 0; at < kOminous.size(); ++at) {
        const wire::nbt::NbtCompound* layer = layers[at].get<wire::nbt::NbtCompound>();
        if (layer == nullptr) {
            return false;
        }
        std::string_view pattern = layer->getString("pattern");
        if (pattern.starts_with("minecraft:")) {
            pattern.remove_prefix(std::string_view{"minecraft:"}.size());
        }
        if (pattern != kOminous[at].first || layer->getString("color") != kOminous[at].second) {
            return false;
        }
    }
    return true;
}

/// What one banner is over there: its own colour and its layers, both counted from the other end of
/// the sixteen dyes.
void writeBanner(wire::io::ByteBuffer& extras, const std::int32_t x, const std::int32_t y, const std::int32_t z,
                 const std::uint8_t colour, const wire::nbt::NbtCompound& data) {
    constexpr std::int32_t kLastDye = 15;

    std::vector<wire::bedrock::BedrockBannerLayer> layers;
    const wire::nbt::NbtList* saved = data.getList("patterns");
    const bool ominous = saved != nullptr && isOminous(*saved);
    if (saved != nullptr && !ominous) {
        for (const wire::nbt::NbtTag& entry : *saved) {
            const wire::nbt::NbtCompound* layer = entry.get<wire::nbt::NbtCompound>();
            if (layer == nullptr) {
                continue;
            }
            layers.push_back({.pattern = wire::bedrock::bedrockBannerPattern(layer->getString("pattern")),
                              .colour = kLastDye - wire::bedrock::dyeIndex(layer->getString("color"))});
        }
    }
    wire::bedrock::writeBannerTag(extras, x, y, z, kLastDye - static_cast<std::int32_t>(colour), layers, ominous,
                                  data.getString("CustomName"));
}

} // namespace

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

    // Where the beds are, picked up while the blocks are being read: this edition keeps a bed's
    // colour in a block entity, and scanning the column a second time for them would be daft.
    struct Bed {
        std::int32_t x = 0;
        std::int32_t y = 0;
        std::int32_t z = 0;
        std::uint8_t colour = 0;
    };

    std::vector<Bed> beds;

    wire::io::ByteBuffer packed{static_cast<std::size_t>(sent) * 1024};
    std::array<std::int32_t, wire::bedrock::kSubChunkBlocks> blocks{};
    for (std::int32_t section = 0; section < sent; ++section) {
        for (std::int32_t y = 0; y < 16; ++y) {
            for (std::int32_t z = 0; z < 16; ++z) {
                for (std::int32_t x = 0; x < 16; ++x) {
                    const std::size_t at = (static_cast<std::size_t>(y) << 8U) | (static_cast<std::size_t>(z) << 4U) |
                                           static_cast<std::size_t>(x);
                    blocks[at] = chunk.blockStateId(x, bottom + (section * 16) + y, z);
                    if (const auto colour = bedColourOf(blocks[at]); colour) {
                        beds.push_back({x, bottom + (section * 16) + y, z, *colour});
                    }
                }
            }
        }
        // Bedrock counts its subchunks from the bottom of the world, which for the overworld starts
        // four below zero: the index is the section's own place, not its distance from y zero.
        wire::bedrock::writeSubChunk(packed, blocks,
                                     static_cast<std::int8_t>(chunk.verticalBounds().minSection() + section), protocol);
    }

    // What the column holds that is more than a block: the signs this edition keeps text in, and
    // the beds it keeps a colour in. The rest are drawn from the block itself over there.
    wire::io::ByteBuffer extras;
    for (const Bed& bed : beds) {
        wire::bedrock::writeBedTag(extras, (chunk.chunkX() * 16) + bed.x, bed.y, (chunk.chunkZ() * 16) + bed.z,
                                   bed.colour);
    }
    for (const server::world::ChunkBlockEntity& entity : chunk.blockEntities()) {
        const std::int32_t at = chunk.blockStateId(entity.localX(), entity.y(), entity.localZ());
        if (const auto colour = bannerColourOf(at); colour) {
            writeBanner(extras, (chunk.chunkX() * 16) + entity.localX(), entity.y(),
                        (chunk.chunkZ() * 16) + entity.localZ(), *colour, entity.data());
            continue;
        }
        if (!world::signs::isSign(at)) {
            continue;
        }
        wire::bedrock::writeSignTag(
            extras, (chunk.chunkX() * 16) + entity.localX(), entity.y(), (chunk.chunkZ() * 16) + entity.localZ(),
            world::signs::plainText(entity.data(), true), world::signs::plainText(entity.data(), false),
            world::signs::isWaxed(entity.data()));
    }

    wire::io::ByteBuffer body;
    // The count of subchunks is what was written; the count of biome sections is the column's own
    // height, which does not shrink with them.
    wire::bedrock::writeLevelChunk(body, chunk.chunkX(), chunk.chunkZ(), static_cast<std::uint32_t>(sent),
                                   packed.readable(), static_cast<std::uint32_t>(sections), 0, protocol,
                                   extras.readable());
    return body.toVector();
}

} // namespace basalt::server::runtime::bedrock
