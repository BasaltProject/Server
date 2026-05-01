#include <basalt/server/registry/World.hpp>
#include <basalt/server/runtime/bedrock/BedrockBlockEntities.hpp>
#include <basalt/server/runtime/bedrock/BedrockChunks.hpp>
#include <basalt/server/runtime/world/Signs.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/ChunkSection.hpp>
#include <basalt/wire/bedrock/BedrockBanners.hpp>
#include <basalt/wire/bedrock/BedrockBiomes.hpp>
#include <basalt/wire/bedrock/BedrockColumn.hpp>
#include <basalt/wire/bedrock/BedrockPackets.hpp>
#include <basalt/wire/io/ByteBuffer.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::bedrock {
namespace {

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

std::vector<std::uint8_t> writeColumn(const server::world::Chunk& chunk, const int protocol) {
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
                    blocks[at] = chunk.blockStateId(x, bottom + (section * 16) + y, z);
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
