#include <basalt/server/runtime/world/RealTimelines.hpp>
#include <basalt/wire/common/json/Json.hpp>
#include <basalt/wire/nbt/JsonToNbt.hpp>

#include <algorithm>
#include <array>
#include <expected>

namespace basalt::server::runtime::world {
namespace {

using wire::common::Identifier;
using wire::nbt::NbtCompound;

constexpr std::string_view kPrefix = "data/minecraft/timeline/";
constexpr std::string_view kJsonSuffix = ".json";
constexpr std::string_view kVanillaPrefix = "minecraft:";

/// parity: the `EnvironmentAttributes` that were built with `syncable()`, and only those.
constexpr auto kSyncableAttributes = std::to_array<std::string_view>({"audio/ambient_sounds",
                                                                      "audio/background_music",
                                                                      "audio/firefly_bush_sounds",
                                                                      "audio/music_volume",
                                                                      "gameplay/can_start_raid",
                                                                      "gameplay/creaking_active",
                                                                      "gameplay/fast_lava",
                                                                      "gameplay/piglins_zombify",
                                                                      "gameplay/sky_light_level",
                                                                      "visual/ambient_light_color",
                                                                      "visual/ambient_particles",
                                                                      "visual/block_light_tint",
                                                                      "visual/cloud_color",
                                                                      "visual/cloud_fog_end_distance",
                                                                      "visual/cloud_height",
                                                                      "visual/default_dripstone_particle",
                                                                      "visual/fog_color",
                                                                      "visual/fog_end_distance",
                                                                      "visual/fog_start_distance",
                                                                      "visual/moon_angle",
                                                                      "visual/moon_phase",
                                                                      "visual/night_vision_color",
                                                                      "visual/sky_color",
                                                                      "visual/sky_fog_end_distance",
                                                                      "visual/sky_light_color",
                                                                      "visual/sky_light_factor",
                                                                      "visual/star_angle",
                                                                      "visual/star_brightness",
                                                                      "visual/sun_angle",
                                                                      "visual/sunrise_sunset_color",
                                                                      "visual/water_fog_color",
                                                                      "visual/water_fog_end_distance",
                                                                      "visual/water_fog_start_distance"});

[[nodiscard]] std::string stripped(const std::string& entry) {
    std::string path = entry;
    if (path.starts_with(kPrefix)) {
        path.erase(0, kPrefix.size());
    }
    if (path.ends_with(kJsonSuffix)) {
        path.resize(path.size() - kJsonSuffix.size());
    }
    return path;
}

/// parity: `Timeline.filterSyncableTracks`, which is the only thing `NETWORK_CODEC` changes.
[[nodiscard]] NbtCompound syncable(const NbtCompound& root) {
    const NbtCompound* tracks = root.getCompound("tracks");
    if (tracks == nullptr) {
        return root;
    }
    NbtCompound kept;
    for (std::size_t index = 0; index < tracks->size(); ++index) {
        std::string_view name = tracks->names()[index];
        if (name.starts_with(kVanillaPrefix)) {
            name.remove_prefix(kVanillaPrefix.size());
        }
        if (std::ranges::find(kSyncableAttributes, name) == kSyncableAttributes.end()) {
            continue;
        }
        kept.set(tracks->names()[index], tracks->values()[index]);
    }
    NbtCompound result = root;
    if (kept.empty()) {
        result.remove("tracks");
    } else {
        result.set("tracks", wire::nbt::NbtTag::ofCompound(std::move(kept)));
    }
    return result;
}

/// One entry as the registry packet wants it, or why it cannot travel as NBT.
[[nodiscard]] std::expected<NbtCompound, std::string> convert(const std::optional<std::string>& text) {
    if (!text.has_value()) {
        return std::unexpected(std::string{"no such entry"});
    }
    const std::expected<wire::common::JsonValue, wire::common::JsonError> json = wire::common::parseJson(*text);
    if (!json.has_value()) {
        return std::unexpected(std::string{"malformed JSON"});
    }
    const std::expected<wire::nbt::NbtTag, wire::nbt::JsonToNbtError> tag = wire::nbt::jsonToNbt(*json);
    if (!tag.has_value()) {
        return std::unexpected(std::string{tag.error() == wire::nbt::JsonToNbtError::NullHasNoEquivalent
                                               ? "a null has no NBT equivalent"
                                               : "a list mixes types"});
    }
    const NbtCompound* compound = tag->get<NbtCompound>();
    if (compound == nullptr) {
        return std::unexpected(std::string{"not a compound"});
    }
    return syncable(*compound);
}

} // namespace

RealTimelines::RealTimelines(const Jar& jar, wire::common::Logger logger) : logger_(std::move(logger)) {
    if (!jar.entriesUnder || !jar.readText) {
        return;
    }
    std::vector<std::string> names = jar.entriesUnder(kPrefix);
    std::sort(names.begin(), names.end());
    for (const std::string& entry : names) {
        if (!entry.ends_with(kJsonSuffix)) {
            continue;
        }
        const std::string path = stripped(entry);
        std::expected<NbtCompound, std::string> converted = convert(jar.readText(entry));
        if (!converted.has_value()) {
            logger_.warn([&] {
                return "Timeline '" + path + "' could not be converted (" + converted.error() +
                       "); sending no timeline registry, because a partial one would disconnect clients at login.";
            });
            entries_.clear();
            return;
        }
        entries_.emplace_back(Identifier::vanilla(path), std::move(*converted));
    }
    if (!entries_.empty()) {
        logger_.info([&] { return "Loaded " + std::to_string(entries_.size()) + " timeline(s)."; });
    }
}

std::vector<Identifier> RealTimelines::ids() const {
    std::vector<Identifier> ids;
    ids.reserve(entries_.size());
    for (const auto& [id, definition] : entries_) {
        ids.push_back(id);
    }
    return ids;
}

const NbtCompound* RealTimelines::definitionOf(const Identifier& id) const {
    for (const auto& [name, definition] : entries_) {
        if (name == id) {
            return &definition;
        }
    }
    return nullptr;
}

} // namespace basalt::server::runtime::world
