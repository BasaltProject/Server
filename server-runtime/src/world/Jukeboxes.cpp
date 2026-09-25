#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/registry/VanillaRegistryData.hpp>
#include <basalt/server/runtime/world/ComparatorReading.hpp>
#include <basalt/server/runtime/world/Jukeboxes.hpp>
#include <basalt/server/runtime/world/RedstoneComponents.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>

#include <charconv>
#include <cmath>
#include <cstdint>
#include <expected>
#include <utility>

namespace basalt::server::runtime::world::jukeboxes {
namespace {

/// parity: `JukeboxBlockEntity.SONG_ITEM_TAG_ID`, an `ItemStack.CODEC` compound: the disc itself,
/// written under vanilla's own name so a world of ours opens in vanilla still playing.
constexpr std::string_view kRecordKey = "RecordItem";

/// parity: `JukeboxBlockEntity.TICKS_SINCE_SONG_STARTED_TAG_ID`, written only while a song plays. A
/// disc that has finished stays in the block, so this is the only thing that says it is still going.
constexpr std::string_view kTicksKey = "ticks_since_song_started";

/// parity: `JukeboxSong.SONG_END_PADDING_TICKS`, a second of silence after the last note.
constexpr std::int64_t kEndPadding = 20;

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

/// The disc a jukebox holds, read the way vanilla writes it.
[[nodiscard]] std::optional<wire::common::Identifier> discIn(const wire::nbt::NbtCompound& data) {
    const wire::nbt::NbtTag* record = data.find(std::string{kRecordKey});
    const wire::nbt::NbtCompound* held = record == nullptr ? nullptr : record->get<wire::nbt::NbtCompound>();
    if (held == nullptr) {
        return std::nullopt;
    }
    const std::string_view id = held->getString("id");
    if (id.empty()) {
        return std::nullopt;
    }
    const std::expected<wire::common::Identifier, wire::common::IdentifierError> parsed =
        wire::common::Identifier::parse(std::string{id});
    return parsed.has_value() ? std::optional{*parsed} : std::nullopt;
}

[[nodiscard]] std::optional<std::int32_t> turned(std::int32_t stateId, const wire::common::Identifier& id,
                                                 const std::string& hasRecord) {
    return server::registry::blockStateDefinitions::stateIdOf(id, {{"has_record", hasRecord}}, stateId);
}

[[nodiscard]] std::string propertyOf(std::int32_t stateId, const std::string& name) {
    const std::map<std::string, std::string> properties =
        server::registry::blockStateDefinitions::propertiesOf(stateId);
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

std::optional<std::int32_t> songNetworkId(const std::string_view song) {
    const std::vector<runtime::registry::vanillaRegistryData::Entry>& songs =
        runtime::registry::vanillaRegistryData::jukeboxSongs();
    const auto found = std::ranges::find(songs, song, &runtime::registry::vanillaRegistryData::Entry::first);
    return found == songs.end() ? std::nullopt : std::optional{static_cast<std::int32_t>(found - songs.begin())};
}

bool isPlayingAt(WorldState& world, const std::int32_t x, const std::int32_t y, const std::int32_t z) {
    const server::world::ChunkBlockEntity* entity = entityAt(world, x, y, z, false);
    return entity != nullptr && isPlaying(entity->data());
}

std::optional<BlockChange> insert(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                  const wire::common::Identifier& disc) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return std::nullopt;
    }
    const wire::common::Identifier* found = server::registry::blockStateDefinitions::blockOf(*stateId);
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
    wire::nbt::NbtCompound record;
    record.putString("id", disc.toString());
    record.putInt("count", 1);
    entity->data().set(std::string{kRecordKey}, wire::nbt::NbtTag::ofCompound(std::move(record)));
    entity->data().putLong(kTicksKey, 0);
    world.chunkIfLoaded(x >> 4, z >> 4)->markDirty();
    redstoneComponents::scheduleComparatorsAround(world, {}, x, y, z, comparatorReading::of(world));
    BlockChange change{.x = x, .y = y, .z = z, .stateId = *playing};
    // parity: `JukeboxSongPlayer.play`, for a disc whose song this server knows.
    if (const std::optional<std::int32_t> song = songNetworkId(songOf(disc)); song.has_value()) {
        change.effects.emplace_back(blockEffect::Effect{.type = blockEffect::Effect::kPlayJukeboxSong, .data = *song});
    }
    return change;
}

std::optional<std::pair<BlockChange, wire::common::Identifier>>
eject(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, const std::optional<std::int64_t> gameTime) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return std::nullopt;
    }
    const wire::common::Identifier* found = server::registry::blockStateDefinitions::blockOf(*stateId);
    if (found == nullptr || *found != block() || propertyOf(*stateId, "has_record") != "true") {
        return std::nullopt;
    }
    server::world::ChunkBlockEntity* entity = entityAt(world, x, y, z, false);
    if (entity == nullptr) {
        return std::nullopt;
    }
    const std::optional<wire::common::Identifier> disc = discIn(entity->data());
    if (!disc.has_value()) {
        return std::nullopt;
    }
    const std::optional<std::int32_t> quiet = turned(*stateId, *found, "false");
    const bool wasPlaying = isPlaying(entity->data());
    if (!quiet.has_value() || !world.setBlockState(x, y, z, *quiet)) {
        return std::nullopt;
    }
    static_cast<void>(entity->data().remove(std::string{kRecordKey}));
    static_cast<void>(entity->data().remove(std::string{kTicksKey}));
    world.chunkIfLoaded(x >> 4, z >> 4)->markDirty();
    redstoneComponents::scheduleComparatorsAround(world, {}, x, y, z, comparatorReading::of(world));
    BlockChange change{.x = x, .y = y, .z = z, .stateId = *quiet};
    if (wasPlaying) {
        change.effects.emplace_back(blockEffect::Effect{.type = blockEffect::Effect::kStopJukeboxSong});
        // parity: `JukeboxSongPlayer.stop`, which an allay dancing to it hears.
        if (gameTime.has_value()) {
            vibrations::emit(world, "jukebox_stop_play", x + 0.5, y + 0.5, z + 0.5, *gameTime);
        }
    }
    return std::pair{change, *disc};
}

std::optional<std::string> songAt(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const server::world::ChunkBlockEntity* entity = entityAt(world, x, y, z, false);
    if (entity == nullptr) {
        return std::nullopt;
    }
    const std::optional<wire::common::Identifier> disc = discIn(entity->data());
    return disc.has_value() ? std::optional{songOf(*disc)} : std::nullopt;
}

bool isPlaying(const wire::nbt::NbtCompound& data) { return data.find(kTicksKey) != nullptr; }

bool tick(WorldState& world, const std::int32_t x, const std::int32_t y, const std::int32_t z,
          const std::int64_t lengthTicks, const std::optional<std::int64_t> gameTime) {
    server::world::ChunkBlockEntity* entity = entityAt(world, x, y, z, false);
    if (entity == nullptr || !isPlaying(entity->data())) {
        return false;
    }
    const std::int64_t elapsed = entity->data().getLong(kTicksKey, 0);
    // parity: `JukeboxSongPlayer.tick` checks whether the song is over **before** counting, so the
    // last turn is spent on the check and not on the counter.
    if (elapsed >= lengthTicks + kEndPadding) {
        entity->data().remove(kTicksKey);
        if (gameTime.has_value()) {
            vibrations::emit(world, "jukebox_stop_play", x + 0.5, y + 0.5, z + 0.5, *gameTime);
        }
        return true;
    }
    // parity: `shouldEmitJukeboxPlayingEvent`, once a second from the start.
    if (gameTime.has_value() && elapsed % 20 == 0) {
        vibrations::emit(world, "jukebox_play", x + 0.5, y + 0.5, z + 0.5, *gameTime);
    }
    entity->data().putLong(kTicksKey, elapsed + 1);
    return false;
}

std::int32_t comparatorOutputAt(const WorldState& world, const wire::nbt::NbtCompound& data) {
    const std::optional<wire::common::Identifier> disc = discIn(data);
    return disc.has_value() ? world.songOutput(songOf(*disc)) : 0;
}

namespace {

/// The number after `"<field>":` in a song's own JSON. Two fields, both numbers, no nesting: a whole
/// JSON reader here would be more machinery than the file has.
[[nodiscard]] std::optional<double> numberIn(const std::string& json, const std::string_view field) {
    const std::size_t at = json.find('"' + std::string{field} + '"');
    const std::size_t colon = at == std::string::npos ? std::string::npos : json.find(':', at);
    if (colon == std::string::npos) {
        return std::nullopt;
    }
    std::size_t start = json.find_first_not_of(' ', colon + 1);
    if (start == std::string::npos) {
        return std::nullopt;
    }
    std::size_t end = start;
    while (end < json.size() && (std::isdigit(static_cast<unsigned char>(json[end])) != 0 || json[end] == '.')) {
        ++end;
    }
    double value = 0.0;
    if (std::from_chars(json.data() + start, json.data() + end, value).ec != std::errc{}) {
        return std::nullopt;
    }
    return value;
}

} // namespace

const SongOutputs::Song& SongOutputs::read(const std::string& song) {
    if (const auto found = cached_.find(song); found != cached_.end()) {
        return found->second;
    }
    Song value;
    const std::optional<std::string> json =
        readText_ ? readText_("data/minecraft/jukebox_song/" + song + ".json") : std::nullopt;
    if (json.has_value()) {
        value.comparatorOutput = static_cast<std::int32_t>(numberIn(*json, "comparator_output").value_or(0.0));
        // parity: `JukeboxSong.lengthInTicks`, seconds rounded up, and the float is the jar's.
        const auto seconds = static_cast<float>(numberIn(*json, "length_in_seconds").value_or(0.0));
        value.lengthTicks = static_cast<std::int64_t>(std::ceil(seconds * 20.0F));
    }
    return cached_.emplace(song, value).first->second;
}

std::int32_t SongOutputs::of(const std::string& song) { return read(song).comparatorOutput; }

std::int64_t SongOutputs::lengthOf(const std::string& song) { return read(song).lengthTicks; }

} // namespace basalt::server::runtime::world::jukeboxes
