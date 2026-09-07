#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/world/Jukeboxes.hpp>

#include <charconv>
#include <cstdint>
#include <utility>

namespace basalt::server::runtime::world::jukeboxes {
namespace {

constexpr std::string_view kSongKey = "BasaltSong";

/// The song's own comparator number, kept beside it so a comparator needs no lookup.
constexpr std::string_view kOutputKey = "BasaltSongOutput";

constexpr std::string_view kDiscPrefix = "music_disc_";

/// The block entity of a jukebox, made on the spot if the block has none yet.
[[nodiscard]] server::world::ChunkBlockEntity* entityAt(WorldState& world, std::int32_t x, std::int32_t y,
                                                        std::int32_t z, bool create) {
    server::world::Chunk* chunk = world.chunkIfLoaded(x >> 4, z >> 4);
    if (chunk == nullptr) {
        return nullptr;
    }
    if (server::world::ChunkBlockEntity* existing = chunk->blockEntityAt(x & 15, y, z & 15); existing != nullptr) {
        return existing;
    }
    if (!create) {
        return nullptr;
    }
    chunk->setBlockEntity(server::world::ChunkBlockEntity{x & 15, y, z & 15, block(), wire::nbt::NbtCompound{}});
    return chunk->blockEntityAt(x & 15, y, z & 15);
}

[[nodiscard]] std::optional<std::int32_t> turned(std::int32_t stateId, const wire::common::Identifier& id,
                                                 const std::string& hasRecord) {
    return registry::blockStateDefinitions::stateIdOf(id, {{"has_record", hasRecord}}, stateId);
}

[[nodiscard]] std::string propertyOf(std::int32_t stateId, const std::string& name) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find(name);
    return found == properties.end() ? std::string{} : found->second;
}

} // namespace

const wire::common::Identifier& block() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("jukebox");
    return instance;
}

std::string songOf(const wire::common::Identifier& disc) {
    const std::string_view path = disc.path();
    return std::string{path.starts_with(kDiscPrefix) ? path.substr(kDiscPrefix.size()) : path};
}

bool isDisc(const wire::common::Identifier& item) { return item.path().starts_with(kDiscPrefix); }

std::optional<BlockChange> insert(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                  const wire::common::Identifier& disc, std::int32_t comparatorOutput) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return std::nullopt;
    }
    const wire::common::Identifier* found = registry::blockStateDefinitions::blockOf(*stateId);
    if (found == nullptr || *found != block() || propertyOf(*stateId, "has_record") == "true") {
        return std::nullopt;
    }
    server::world::ChunkBlockEntity* entity = entityAt(world, x, y, z, true);
    if (entity == nullptr) {
        return std::nullopt;
    }
    const std::optional<std::int32_t> playing = turned(*stateId, *found, "true");
    if (!playing.has_value() || !world.setBlockState(x, y, z, *playing)) {
        return std::nullopt;
    }
    entity->data().putString(kSongKey, songOf(disc));
    entity->data().putInt(kOutputKey, comparatorOutput);
    world.chunkIfLoaded(x >> 4, z >> 4)->markDirty();
    return BlockChange{.x = x, .y = y, .z = z, .stateId = *playing};
}

std::optional<std::pair<BlockChange, wire::common::Identifier>> eject(WorldState& world, std::int32_t x, std::int32_t y,
                                                                      std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return std::nullopt;
    }
    const wire::common::Identifier* found = registry::blockStateDefinitions::blockOf(*stateId);
    if (found == nullptr || *found != block() || propertyOf(*stateId, "has_record") != "true") {
        return std::nullopt;
    }
    server::world::ChunkBlockEntity* entity = entityAt(world, x, y, z, false);
    if (entity == nullptr) {
        return std::nullopt;
    }
    const std::string song{entity->data().getString(kSongKey)};
    if (song.empty()) {
        return std::nullopt;
    }
    const std::optional<std::int32_t> quiet = turned(*stateId, *found, "false");
    if (!quiet.has_value() || !world.setBlockState(x, y, z, *quiet)) {
        return std::nullopt;
    }
    entity->data().remove(kSongKey);
    entity->data().remove(kOutputKey);
    world.chunkIfLoaded(x >> 4, z >> 4)->markDirty();
    return std::pair{BlockChange{.x = x, .y = y, .z = z, .stateId = *quiet},
                     wire::common::Identifier::vanilla(std::string{kDiscPrefix} + song)};
}

std::optional<std::string> songAt(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const server::world::ChunkBlockEntity* entity = entityAt(world, x, y, z, false);
    if (entity == nullptr) {
        return std::nullopt;
    }
    const std::string_view song = entity->data().getString(kSongKey);
    return song.empty() ? std::nullopt : std::optional{std::string{song}};
}

std::int32_t comparatorOutputAt(const wire::nbt::NbtCompound& data) { return data.getInt(kOutputKey, 0); }

std::int32_t SongOutputs::of(const std::string& song) {
    if (const auto found = cached_.find(song); found != cached_.end()) {
        return found->second;
    }
    std::int32_t value = 0;
    const std::optional<std::string> json =
        readText_ ? readText_("data/minecraft/jukebox_song/" + song + ".json") : std::nullopt;
    if (json.has_value()) {
        const std::size_t at = json->find("\"comparator_output\"");
        const std::size_t colon = at == std::string::npos ? std::string::npos : json->find(':', at);
        if (colon != std::string::npos) {
            std::size_t start = colon + 1;
            while (start < json->size() && (*json)[start] == ' ') {
                ++start;
            }
            std::size_t end = start;
            while (end < json->size() && std::isdigit(static_cast<unsigned char>((*json)[end])) != 0) {
                ++end;
            }
            std::from_chars(json->data() + start, json->data() + end, value);
        }
    }
    cached_.emplace(song, value);
    return value;
}

} // namespace basalt::server::runtime::world::jukeboxes
