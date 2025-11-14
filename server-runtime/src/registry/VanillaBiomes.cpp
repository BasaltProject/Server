#include <basalt/server/runtime/registry/VanillaBiomes.hpp>
#include <basalt/server/runtime/registry/VanillaOverworldBiomes.hpp>

#include <utility>

namespace basalt::server::runtime::registry::vanillaBiomes {
namespace {

using wire::nbt::NbtCompound;
using wire::nbt::NbtList;
using wire::nbt::NbtTag;
using wire::nbt::NbtType;

void writeAttribute(NbtCompound& into, const std::string& key, const BiomeAttribute& value) {
    switch (value.kind) {
    case BiomeAttributeKind::Color:
        into.putString(key, value.text);
        return;
    case BiomeAttributeKind::Flag:
        into.putBoolean(key, value.flag);
        return;
    case BiomeAttributeKind::Number:
        into.putFloat(key, value.number);
        return;
    case BiomeAttributeKind::Modifier: {
        NbtCompound written;
        written.putFloat("argument", value.number);
        written.putString("modifier", value.text);
        into.set(key, NbtTag::ofCompound(std::move(written)));
        return;
    }
    case BiomeAttributeKind::Music: {
        NbtCompound written;
        for (const auto& [slot, track] : value.tracks) {
            NbtCompound one;
            one.putString("sound", track.sound);
            one.putInt("min_delay", track.minDelay);
            one.putInt("max_delay", track.maxDelay);
            written.set(slot, NbtTag::ofCompound(std::move(one)));
        }
        into.set(key, NbtTag::ofCompound(std::move(written)));
        return;
    }
    case BiomeAttributeKind::Particles: {
        NbtList written{NbtType::Compound};
        for (const AmbientParticle& particle : value.particles) {
            NbtCompound type;
            type.putString("type", particle.type);
            NbtCompound one;
            one.set("particle", NbtTag::ofCompound(std::move(type)));
            one.putFloat("probability", particle.probability);
            written.push(NbtTag::ofCompound(std::move(one)));
        }
        into.set(key, NbtTag::ofList(std::move(written)));
        return;
    }
    case BiomeAttributeKind::Sounds:
    default: {
        NbtCompound written;
        written.putString("loop", value.loop);
        NbtCompound mood;
        mood.putString("sound", value.mood.sound);
        mood.putInt("tick_delay", value.mood.tickDelay);
        mood.putInt("block_search_extent", value.mood.blockSearchExtent);
        mood.putDouble("offset", value.mood.offset);
        written.set("mood", NbtTag::ofCompound(std::move(mood)));
        NbtCompound additions;
        additions.putString("sound", value.addition.sound);
        additions.putDouble("tick_chance", value.addition.tickChance);
        written.set("additions", NbtTag::ofCompound(std::move(additions)));
        into.set(key, NbtTag::ofCompound(std::move(written)));
        return;
    }
    }
}

} // namespace

BiomeAttribute BiomeAttribute::color(std::string hex) {
    return BiomeAttribute{.kind = BiomeAttributeKind::Color, .text = std::move(hex)};
}

BiomeAttribute BiomeAttribute::boolean(bool value) {
    return BiomeAttribute{.kind = BiomeAttributeKind::Flag, .flag = value};
}

BiomeAttribute BiomeAttribute::real(float value) {
    return BiomeAttribute{.kind = BiomeAttributeKind::Number, .number = value};
}

BiomeAttribute BiomeAttribute::modifier(float argument, std::string modifier) {
    return BiomeAttribute{.kind = BiomeAttributeKind::Modifier, .text = std::move(modifier), .number = argument};
}

BiomeAttribute BiomeAttribute::music(std::vector<std::pair<std::string, MusicTrack>> tracks) {
    return BiomeAttribute{.kind = BiomeAttributeKind::Music, .tracks = std::move(tracks)};
}

BiomeAttribute BiomeAttribute::ambientParticles(std::vector<AmbientParticle> particles) {
    return BiomeAttribute{.kind = BiomeAttributeKind::Particles, .particles = std::move(particles)};
}

BiomeAttribute BiomeAttribute::sounds(std::string loop, AmbientMood mood, AmbientAddition addition) {
    return BiomeAttribute{.kind = BiomeAttributeKind::Sounds,
                          .loop = std::move(loop),
                          .mood = std::move(mood),
                          .addition = std::move(addition)};
}

const std::vector<Entry>& all() {
    static const std::vector<Entry> built = [] {
        std::vector<Entry> every;
        for (const std::vector<Entry>* group :
             {&overworldBiomesA(), &overworldBiomesB(), &netherBiomes(), &endBiomes()}) {
            every.insert(every.end(), group->begin(), group->end());
        }
        return every;
    }();
    return built;
}

NbtCompound toNbt(const Entry& entry) {
    NbtCompound value;
    value.putBoolean("has_precipitation", entry.hasPrecipitation);
    value.putFloat("temperature", entry.temperature);
    if (entry.temperatureModifier.has_value()) {
        value.putString("temperature_modifier", *entry.temperatureModifier);
    }
    value.putFloat("downfall", entry.downfall);
    if (!entry.attributes.empty()) {
        NbtCompound attributes;
        for (const auto& [key, attribute] : entry.attributes) {
            writeAttribute(attributes, key, attribute);
        }
        value.set("attributes", NbtTag::ofCompound(std::move(attributes)));
    }
    NbtCompound effects;
    effects.putString("water_color", entry.waterColor);
    if (entry.foliageColor.has_value()) {
        effects.putString("foliage_color", *entry.foliageColor);
    }
    if (entry.dryFoliageColor.has_value()) {
        effects.putString("dry_foliage_color", *entry.dryFoliageColor);
    }
    if (entry.grassColor.has_value()) {
        effects.putString("grass_color", *entry.grassColor);
    }
    if (entry.grassColorModifier.has_value()) {
        effects.putString("grass_color_modifier", *entry.grassColorModifier);
    }
    value.set("effects", NbtTag::ofCompound(std::move(effects)));
    return value;
}

} // namespace basalt::server::runtime::registry::vanillaBiomes
