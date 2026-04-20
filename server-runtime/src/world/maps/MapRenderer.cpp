#include <basalt/server/registry/BlockFluids.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/MapColors.hpp>
#include <basalt/server/runtime/world/maps/MapRenderer.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <array>
#include <cstddef>
#include <vector>

namespace basalt::server::runtime::world::maps::mapRenderer {
namespace {

namespace reg = server::registry;
namespace colors = server::registry::mapColors;
using colors::Brightness;

constexpr std::int32_t kHalf = MapData::kSize / 2;

/// parity: the `LinkedHashMultiset` the scan counts colours in.
class ColorCount {
public:
    void add(std::int32_t color, std::int32_t times = 1) {
        for (std::size_t i = 0; i < used_; ++i) {
            if (entries_[i].first == color) {
                entries_[i].second += times;
                return;
            }
        }
        if (used_ < entries_.size()) {
            entries_[used_++] = {color, times};
        }
    }

    [[nodiscard]] std::int32_t mostFrequent() const {
        std::int32_t best = colors::kNone;
        std::int32_t count = 0;
        for (std::size_t i = 0; i < used_; ++i) {
            if (entries_[i].second > count) {
                best = entries_[i].first;
                count = entries_[i].second;
            }
        }
        return best;
    }

private:
    std::array<std::pair<std::int32_t, std::int32_t>, 256> entries_{};
    std::size_t used_ = 0;
};

/// parity: `getCorrectStateForFluidBlock`, as the colour it ends up drawing.
[[nodiscard]] std::int32_t colorForFluidBlock(std::int32_t stateId) {
    if (!reg::blockFluids::hasFluid(stateId) || reg::blockShapes::isFaceSturdy(stateId, reg::BlockFace::Up)) {
        return colors::of(stateId);
    }
    return colors::of(reg::blockFluids::hasWaterFluid(stateId) ? reg::blocks::water().stateId
                                                               : reg::blocks::lava().stateId);
}

[[nodiscard]] Brightness waterBrightness(std::int32_t depth, std::int32_t imgX, std::int32_t imgY) {
    const double diff = (depth * 0.1) + (((imgX + imgY) & 1) * 0.2);
    if (diff < 0.5) {
        return Brightness::High;
    }
    return diff > 0.9 ? Brightness::Low : Brightness::Normal;
}

[[nodiscard]] Brightness slopeBrightness(double height, double previous, std::int32_t scale, std::int32_t imgX,
                                         std::int32_t imgY) {
    const double diff = ((height - previous) * 4.0 / static_cast<double>(scale + 4)) +
                        ((static_cast<double>((imgX + imgY) & 1) - 0.5) * 0.4);
    if (diff > 0.6) {
        return Brightness::High;
    }
    return diff < -0.6 ? Brightness::Low : Brightness::Normal;
}

} // namespace

void update(MapData& data, MapData::Holder& holder, const ScanWorld& world, double playerX, double playerZ) {
    const std::int32_t scale = 1 << data.scale();
    const std::int32_t centerX = data.centerX();
    const std::int32_t centerZ = data.centerZ();
    const std::int32_t playerImgX = (wire::common::mth::floor(playerX - centerX) / scale) + kHalf;
    const std::int32_t playerImgY = (wire::common::mth::floor(playerZ - centerZ) / scale) + kHalf;
    std::int32_t radius = MapData::kSize / scale;
    if (world.hasCeiling) {
        radius /= 2;
    }
    ++holder.step;
    const std::int32_t dirtState = reg::blocks::named("dirt") == nullptr ? 0 : reg::blocks::named("dirt")->stateId;
    const std::int32_t bedrockColor = colors::of(reg::blocks::bedrock().stateId);
    bool foundConsecutiveChanges = false;
    for (std::int32_t imgX = playerImgX - radius + 1; imgX < playerImgX + radius; ++imgX) {
        if ((imgX & 15) != (holder.step & 15) && !foundConsecutiveChanges) {
            continue;
        }
        foundConsecutiveChanges = false;
        double previousHeight = 0.0;
        for (std::int32_t imgY = playerImgY - radius - 1; imgY < playerImgY + radius; ++imgY) {
            if (imgX < 0 || imgY < -1 || imgX >= MapData::kSize || imgY >= MapData::kSize) {
                continue;
            }
            const std::int32_t distanceSqr =
                ((imgX - playerImgX) * (imgX - playerImgX)) + ((imgY - playerImgY) * (imgY - playerImgY));
            const bool ditherBlack = distanceSqr > (radius - 2) * (radius - 2);
            const std::int32_t minX = ((centerX / scale) + imgX - kHalf) * scale;
            const std::int32_t minZ = ((centerZ / scale) + imgY - kHalf) * scale;
            const server::world::Chunk* chunk = world.chunkAt(minX >> 4, minZ >> 4);
            if (chunk == nullptr) {
                continue;
            }
            ColorCount count;
            std::int32_t waterDepth = 0;
            double height = 0.0;
            if (world.hasCeiling) {
                // parity: the Nether's roof is drawn as noise of dirt and stone, not scanned.
                const std::uint32_t seed =
                    static_cast<std::uint32_t>(minX) + (static_cast<std::uint32_t>(minZ) * 231871U);
                const auto noise = static_cast<std::int32_t>((seed * seed * 31287121U) + (seed * 11U));
                if (((noise >> 20) & 1) == 0) {
                    count.add(colors::of(dirtState), 10);
                } else {
                    count.add(colors::of(reg::blocks::stone().stateId), 100);
                }
                height = 100.0;
            } else {
                for (std::int32_t dx = 0; dx < scale; ++dx) {
                    for (std::int32_t dz = 0; dz < scale; ++dz) {
                        const std::int32_t x = minX + dx;
                        const std::int32_t z = minZ + dz;
                        std::int32_t columnY =
                            chunk->heightAt(x & 15, z & 15, server::world::HeightmapType::WorldSurface);
                        std::int32_t color = bedrockColor;
                        if (columnY > world.minY) {
                            std::int32_t state = 0;
                            do {
                                --columnY;
                                state = chunk->blockStateId(x & 15, columnY, z & 15);
                            } while (colors::of(state) == colors::kNone && columnY > world.minY);
                            color = colors::of(state);
                            if (columnY > world.minY && reg::blockFluids::hasFluid(state)) {
                                std::int32_t solidY = columnY - 1;
                                std::int32_t below = 0;
                                do {
                                    below = chunk->blockStateId(x & 15, solidY--, z & 15);
                                    ++waterDepth;
                                } while (solidY > world.minY && reg::blockFluids::hasFluid(below));
                                color = colorForFluidBlock(state);
                            }
                        }
                        data.checkBanners(x, z, world.bannerAt);
                        height += static_cast<double>(columnY) / static_cast<double>(scale * scale);
                        count.add(color);
                    }
                }
            }
            waterDepth /= scale * scale;
            const std::int32_t color = count.mostFrequent();
            const Brightness brightness = color == colors::kWater
                                              ? waterBrightness(waterDepth, imgX, imgY)
                                              : slopeBrightness(height, previousHeight, scale, imgX, imgY);
            previousHeight = height;
            if (imgY >= 0 && distanceSqr < radius * radius && (!ditherBlack || ((imgX + imgY) & 1) != 0)) {
                foundConsecutiveChanges |= data.updateColor(imgX, imgY, colors::packed(color, brightness));
            }
        }
    }
}

void renderBiomePreview(MapData& data, std::int32_t seaLevel,
                        const std::function<bool(std::int32_t x, std::int32_t y, std::int32_t z)>& watery) {
    const std::int32_t scale = 1 << data.scale();
    const std::int32_t startX = (data.centerX() / scale) - kHalf;
    const std::int32_t startZ = (data.centerZ() / scale) - kHalf;
    std::vector<bool> isWatery(static_cast<std::size_t>(MapData::kSize * MapData::kSize));
    for (std::int32_t row = 0; row < MapData::kSize; ++row) {
        for (std::int32_t column = 0; column < MapData::kSize; ++column) {
            isWatery[static_cast<std::size_t>((row * MapData::kSize) + column)] =
                watery((startX + column) * scale, seaLevel, (startZ + row) * scale);
        }
    }
    const auto at = [&isWatery](std::int32_t x, std::int32_t z) {
        return isWatery[static_cast<std::size_t>((z * MapData::kSize) + x)];
    };
    for (std::int32_t mx = 1; mx < MapData::kSize - 1; ++mx) {
        for (std::int32_t mz = 1; mz < MapData::kSize - 1; ++mz) {
            std::int32_t waterCount = 0;
            for (std::int32_t dx = -1; dx < 2; ++dx) {
                for (std::int32_t dz = -1; dz < 2; ++dz) {
                    if ((dx != 0 || dz != 0) && at(mx + dx, mz + dz)) {
                        ++waterCount;
                    }
                }
            }
            Brightness brightness = Brightness::Lowest;
            std::int32_t color = colors::kNone;
            if (at(mx, mz)) {
                color = colors::kColorOrange;
                if (waterCount > 7 && mz % 2 == 0) {
                    const auto wave = static_cast<std::int32_t>(
                        wire::common::mth::sin(static_cast<double>(static_cast<float>(mz) + 0.0F)) * 7.0F);
                    switch ((mx + wave) / 8 % 5) {
                    case 0:
                    case 4:
                        brightness = Brightness::Low;
                        break;
                    case 1:
                    case 3:
                        brightness = Brightness::Normal;
                        break;
                    case 2:
                        brightness = Brightness::High;
                        break;
                    default:
                        break;
                    }
                } else if (waterCount > 7) {
                    color = colors::kNone;
                } else if (waterCount > 5) {
                    brightness = Brightness::Normal;
                } else if (waterCount > 1) {
                    brightness = Brightness::Low;
                }
            } else if (waterCount > 0) {
                color = colors::kColorBrown;
                brightness = waterCount > 3 ? Brightness::Normal : Brightness::Lowest;
            }
            if (color != colors::kNone) {
                data.setColor(mx, mz, colors::packed(color, brightness));
            }
        }
    }
}

} // namespace basalt::server::runtime::world::maps::mapRenderer
