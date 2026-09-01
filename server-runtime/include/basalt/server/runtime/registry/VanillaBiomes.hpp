#pragma once

#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace basalt::server::runtime::registry {

/// Every biome as a 26.2 server syncs it during Configuration: only what `Biome.NETWORK_CODEC`
/// carries, since spawners, features and carvers stay server-side. Entry order matches the block
/// registry's own biome order, because each entry's index is the id a chunk's biome palette
/// references.
///
/// Colours serialize as `#rrggbb`, which is what `ExtraCodecs.STRING_RGB_COLOR` writes; it also
/// decodes plain ints, which is why the earlier int-encoded form worked anyway.
namespace vanillaBiomes {

struct MusicTrack {
    std::string sound;
    std::int32_t minDelay = 0;
    std::int32_t maxDelay = 0;
};

struct AmbientParticle {
    std::string type;
    float probability = 0.0F;
};

struct AmbientMood {
    std::string sound;
    std::int32_t tickDelay = 6000;
    std::int32_t blockSearchExtent = 8;
    double offset = 2.0;
};

struct AmbientAddition {
    std::string sound;
    double tickChance = 0.0111;
};

/// One environment attribute value, in the shapes the real biome data actually uses. One tagged
/// struct rather than a hierarchy: without RTTI the kind is the tag, and these are data.
enum class BiomeAttributeKind : std::uint8_t { Color, Flag, Number, Modifier, Music, Particles, Sounds };

struct BiomeAttribute {
    BiomeAttributeKind kind = BiomeAttributeKind::Color;
    std::string text;
    bool flag = false;
    float number = 0.0F;
    /// The music slots in the order they were written, which is the order they go out in.
    std::vector<std::pair<std::string, MusicTrack>> tracks;
    std::vector<AmbientParticle> particles;
    std::string loop;
    AmbientMood mood;
    AmbientAddition addition;

    [[nodiscard]] static BiomeAttribute color(std::string hex);
    [[nodiscard]] static BiomeAttribute boolean(bool value);
    [[nodiscard]] static BiomeAttribute real(float value);
    /// A real attribute modifier object, such as `visual/water_fog_end_distance`.
    [[nodiscard]] static BiomeAttribute modifier(float argument, std::string modifier);
    [[nodiscard]] static BiomeAttribute music(std::vector<std::pair<std::string, MusicTrack>> tracks);
    [[nodiscard]] static BiomeAttribute ambientParticles(std::vector<AmbientParticle> particles);
    [[nodiscard]] static BiomeAttribute sounds(std::string loop, AmbientMood mood, AmbientAddition addition);
};

struct Entry {
    std::string path;
    float temperature = 0.0F;
    float downfall = 0.0F;
    bool hasPrecipitation = false;
    std::string waterColor;
    std::optional<std::string> temperatureModifier;
    std::optional<std::string> grassColor;
    std::optional<std::string> foliageColor;
    std::optional<std::string> dryFoliageColor;
    std::optional<std::string> grassColorModifier;
    /// In the order they were written, which is the order the codec reads them back.
    std::vector<std::pair<std::string, BiomeAttribute>> attributes;
};

/// Every registered biome, in registration order: overworld first, then the nether, then the end.
/// That sequence is the network id a chunk's biome palette references, so it is pinned rather than
/// sorted.
[[nodiscard]] const std::vector<Entry>& all();

/// One entry as the exact NBT a vanilla server sends for it in the registry data packet.
[[nodiscard]] wire::nbt::NbtCompound toNbt(const Entry& entry);

} // namespace vanillaBiomes

} // namespace basalt::server::runtime::registry
