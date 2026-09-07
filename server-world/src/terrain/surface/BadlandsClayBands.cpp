#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/surface/BadlandsClayBands.hpp>

#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace basalt::server::world::terrain::surface {
namespace {

[[nodiscard]] std::int32_t state(std::string_view name) {
    const registry::BlockState* found = registry::blocks::named(name);
    if (found == nullptr) {
        throw std::runtime_error{"unknown block minecraft:" + std::string{name}};
    }
    return found->stateId;
}

void makeBands(random::RandomSource& source, std::array<std::int32_t, 192>& bands, std::int32_t baseWidth,
               std::int32_t stateId) {
    const std::int32_t runs = source.nextIntBetweenInclusive(6, 15);
    for (std::int32_t run = 0; run < runs; ++run) {
        const std::int32_t width = baseWidth + source.nextInt(3);
        const std::int32_t start = source.nextInt(static_cast<std::int32_t>(bands.size()));
        std::int32_t offset = 0;
        while (start + offset < static_cast<std::int32_t>(bands.size()) && offset < width) {
            bands[static_cast<std::size_t>(start + offset)] = stateId;
            ++offset;
        }
    }
}

} // namespace

BadlandsClayBands::BadlandsClayBands(random::RandomSource& source) {
    const std::int32_t terracotta = state("terracotta");
    bands_.fill(terracotta);

    std::int32_t index = 0;
    while (index < static_cast<std::int32_t>(bands_.size())) {
        index += source.nextInt(5) + 1;
        if (index < static_cast<std::int32_t>(bands_.size())) {
            bands_[static_cast<std::size_t>(index)] = state("orange_terracotta");
        }
        ++index;
    }
    makeBands(source, bands_, 1, state("yellow_terracotta"));
    makeBands(source, bands_, 2, state("brown_terracotta"));
    makeBands(source, bands_, 1, state("red_terracotta"));

    const std::int32_t whiteBandCount = source.nextIntBetweenInclusive(9, 15);
    std::int32_t placed = 0;
    std::int32_t start = 0;
    while (placed < whiteBandCount && start < static_cast<std::int32_t>(bands_.size())) {
        bands_[static_cast<std::size_t>(start)] = state("white_terracotta");
        if (start - 1 > 0 && source.nextBoolean()) {
            bands_[static_cast<std::size_t>(start - 1)] = state("light_gray_terracotta");
        }
        if (start + 1 < static_cast<std::int32_t>(bands_.size()) && source.nextBoolean()) {
            bands_[static_cast<std::size_t>(start + 1)] = state("light_gray_terracotta");
        }
        ++placed;
        start += source.nextInt(16) + 4;
    }
}

std::int32_t BadlandsClayBands::stateId(std::int32_t y, double offsetNoise) const {
    const auto offset = static_cast<std::int32_t>(std::floor(offsetNoise * 4.0 + 0.5));
    const auto size = static_cast<std::int32_t>(bands_.size());
    return bands_[static_cast<std::size_t>((y + offset + size) % size)];
}

} // namespace basalt::server::world::terrain::surface
