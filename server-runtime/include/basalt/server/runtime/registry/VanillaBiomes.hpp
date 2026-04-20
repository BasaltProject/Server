#pragma once

#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace basalt::server::runtime::registry {

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

enum class BiomeAttributeKind : std::uint8_t { Color, Flag, Number, Modifier, Music, Particles, Sounds };

struct BiomeAttribute {
    BiomeAttributeKind kind = BiomeAttributeKind::Color;
    std::string text;
    bool flag = false;
    float number = 0.0F;
    std::vector<std::pair<std::string, MusicTrack>> tracks;
    std::vector<AmbientParticle> particles;
    std::string loop;
    AmbientMood mood;
    AmbientAddition addition;

    [[nodiscard]] static BiomeAttribute color(std::string hex);
    [[nodiscard]] static BiomeAttribute boolean(bool value);
    [[nodiscard]] static BiomeAttribute real(float value);
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
    std::vector<std::pair<std::string, BiomeAttribute>> attributes;
};

[[nodiscard]] const std::vector<Entry>& all();

[[nodiscard]] wire::nbt::NbtCompound toNbt(const Entry& entry);

} // namespace vanillaBiomes

} // namespace basalt::server::runtime::registry
