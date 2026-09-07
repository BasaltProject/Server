#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/DesertWellDecoration.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace basalt::server::world::terrain::decoration {
namespace {

constexpr std::int32_t kSurfaceStructuresStep = 4;
constexpr std::int32_t kGlobalIndex = 2;

constexpr std::array<std::pair<std::int32_t, std::int32_t>, 4> kHorizontalDirections = {
    {{0, -1}, {1, 0}, {0, 1}, {-1, 0}}};

[[nodiscard]] std::int32_t stateIdOf(std::string_view path) {
    const registry::BlockState* found = registry::blocks::named(path);
    if (found == nullptr) {
        throw std::runtime_error{"missing vanilla block minecraft:" + std::string{path}};
    }
    return found->stateId;
}

/// parity: El pozo abarca varios bloques alrededor de su origen, asi que cae sobre el borde a menudo.
/// Vanilla escribe ahi por el `WorldGenLevel`; recortar pierde el trozo que asoma. Coordenadas de mundo.
[[nodiscard]] std::int32_t get(const Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z) {
    const ChunkHeight bounds = chunk.verticalBounds();
    if (y < bounds.minY() || y > bounds.maxY()) {
        return registry::blocks::air().stateId;
    }
    return chunk.decorationBlockStateId(x, y, z);
}

void set(Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId) {
    const ChunkHeight bounds = chunk.verticalBounds();
    if (y >= bounds.minY() && y <= bounds.maxY()) {
        chunk.setDecorationBlockStateId(x, y, z, stateId);
    }
}

[[nodiscard]] bool isAir(const Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z) {
    return registry::blocks::isAir(get(chunk, x, y, z));
}

/// parity: `BlockPos.asLong`, which is what seeds the buried loot.
[[nodiscard]] std::int64_t packedBlockPosition(std::int32_t x, std::int32_t y, std::int32_t z) {
    return ((static_cast<std::int64_t>(x) & 0x3FFFFFFLL) << 38) | ((static_cast<std::int64_t>(z) & 0x3FFFFFFLL) << 12) |
           (static_cast<std::int64_t>(y) & 0xFFFLL);
}

/// Coordinates are world ones, like the rest of the well.
void placeSuspiciousSand(Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z) {
    const ChunkHeight bounds = chunk.verticalBounds();
    if (y < bounds.minY() || y > bounds.maxY()) {
        return;
    }
    const std::int32_t stateId = stateIdOf("suspicious_sand");
    chunk.setDecorationBlockStateId(x, y, z, stateId);
    wire::nbt::NbtCompound data;
    data.putString("LootTable", "minecraft:archaeology/desert_well");
    data.putLong("LootTableSeed", packedBlockPosition(x, y, z));
    // The brushable block entity travels with its sand, so one that landed in a neighbour still
    // carries its loot table when that chunk arrives.
    chunk.setDecorationBlockEntity(x, y, z, stateId, wire::common::Identifier::vanilla("brushable_block"),
                                   std::move(data));
}

} // namespace

void desertWellDecoration::placeAt(Chunk& chunk, random::RandomSource& random, std::int32_t originX,
                                   std::int32_t suppliedY, std::int32_t originZ) {
    const ChunkHeight bounds = chunk.verticalBounds();
    std::int32_t originY = suppliedY + 1;
    while (isAir(chunk, originX, originY, originZ) && originY > bounds.minY() + 2) {
        --originY;
    }
    if (get(chunk, originX, originY, originZ) != stateIdOf("sand")) {
        return;
    }
    // Refuses to build over a hollow: any column in the 5 by 5 footprint with two air blocks under
    // it aborts the whole well.
    for (std::int32_t offsetX = -2; offsetX <= 2; ++offsetX) {
        for (std::int32_t offsetZ = -2; offsetZ <= 2; ++offsetZ) {
            if (isAir(chunk, originX + offsetX, originY - 1, originZ + offsetZ) &&
                isAir(chunk, originX + offsetX, originY - 2, originZ + offsetZ)) {
                return;
            }
        }
    }

    for (std::int32_t offsetY = -2; offsetY <= 0; ++offsetY) {
        for (std::int32_t offsetX = -2; offsetX <= 2; ++offsetX) {
            for (std::int32_t offsetZ = -2; offsetZ <= 2; ++offsetZ) {
                set(chunk, originX + offsetX, originY + offsetY, originZ + offsetZ, stateIdOf("sandstone"));
            }
        }
    }
    set(chunk, originX, originY, originZ, registry::blocks::water().stateId);
    for (const auto& [offsetX, offsetZ] : kHorizontalDirections) {
        set(chunk, originX + offsetX, originY, originZ + offsetZ, registry::blocks::water().stateId);
    }
    set(chunk, originX, originY - 1, originZ, stateIdOf("sand"));
    for (const auto& [offsetX, offsetZ] : kHorizontalDirections) {
        set(chunk, originX + offsetX, originY - 1, originZ + offsetZ, stateIdOf("sand"));
    }

    for (std::int32_t offsetX = -2; offsetX <= 2; ++offsetX) {
        for (std::int32_t offsetZ = -2; offsetZ <= 2; ++offsetZ) {
            if (offsetX == -2 || offsetX == 2 || offsetZ == -2 || offsetZ == 2) {
                set(chunk, originX + offsetX, originY + 1, originZ + offsetZ, stateIdOf("sandstone"));
            }
        }
    }
    set(chunk, originX + 2, originY + 1, originZ, stateIdOf("sandstone_slab"));
    set(chunk, originX - 2, originY + 1, originZ, stateIdOf("sandstone_slab"));
    set(chunk, originX, originY + 1, originZ + 2, stateIdOf("sandstone_slab"));
    set(chunk, originX, originY + 1, originZ - 2, stateIdOf("sandstone_slab"));

    for (std::int32_t offsetX = -1; offsetX <= 1; ++offsetX) {
        for (std::int32_t offsetZ = -1; offsetZ <= 1; ++offsetZ) {
            set(chunk, originX + offsetX, originY + 4, originZ + offsetZ,
                offsetX == 0 && offsetZ == 0 ? stateIdOf("sandstone") : stateIdOf("sandstone_slab"));
        }
    }
    for (std::int32_t offsetY = 1; offsetY <= 3; ++offsetY) {
        set(chunk, originX - 1, originY + offsetY, originZ - 1, stateIdOf("sandstone"));
        set(chunk, originX - 1, originY + offsetY, originZ + 1, stateIdOf("sandstone"));
        set(chunk, originX + 1, originY + offsetY, originZ - 1, stateIdOf("sandstone"));
        set(chunk, originX + 1, originY + offsetY, originZ + 1, stateIdOf("sandstone"));
    }

    static constexpr std::array<std::pair<std::int32_t, std::int32_t>, 5> kWaterOffsets = {
        {{0, 0}, {1, 0}, {0, 1}, {-1, 0}, {0, -1}}};
    const auto& shallow = kWaterOffsets[static_cast<std::size_t>(random.nextInt(kWaterOffsets.size()))];
    placeSuspiciousSand(chunk, originX + shallow.first, originY - 1, originZ + shallow.second);
    const auto& deep = kWaterOffsets[static_cast<std::size_t>(random.nextInt(kWaterOffsets.size()))];
    placeSuspiciousSand(chunk, originX + deep.first, originY - 2, originZ + deep.second);
}

void desertWellDecoration::decorate(Chunk& chunk, std::int64_t seed,
                                    const surface::SurfaceRuleEnvironment& environment) {
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    const std::int64_t decorationSeed =
        random::setDecorationSeed(random, seed, chunk.chunkX() * 16, chunk.chunkZ() * 16);
    random::setFeatureSeed(random, decorationSeed, kGlobalIndex, kSurfaceStructuresStep);
    if (random.nextFloat() >= 1.0F / 1000.0F) {
        return;
    }
    const std::int32_t localX = random.nextInt(16);
    const std::int32_t localZ = random.nextInt(16);
    const std::int32_t suppliedY = chunk.heightAt(localX, localZ, HeightmapType::WorldSurface);
    const std::int32_t worldX = chunk.chunkX() * 16 + localX;
    const std::int32_t worldZ = chunk.chunkZ() * 16 + localZ;
    const std::string& biome = environment.biomeAt(worldX, suppliedY, worldZ);
    const std::size_t colon = biome.find(':');
    if ((colon == std::string::npos ? biome : biome.substr(colon + 1)) != "desert") {
        return;
    }
    placeAt(chunk, random, worldX, suppliedY, worldZ);
}

} // namespace basalt::server::world::terrain::decoration
