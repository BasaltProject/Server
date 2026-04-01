#pragma once

#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace basalt::server::runtime::world {

/// The jukebox. parity: `JukeboxBlock` over `JukeboxBlockEntity`: while a disc plays the block gives
/// a full signal and a comparator reads the song's number, read from the operator's jar
/// (`data/minecraft/jukebox_song/<name>.json`).
namespace jukeboxes {

[[nodiscard]] const wire::common::Identifier& block();

/// parity: the disc item's own `jukebox_playable`, which names the song it plays.
[[nodiscard]] std::string songOf(const wire::common::Identifier& disc);

/// Whether this item is a disc a jukebox takes.
[[nodiscard]] bool isDisc(const wire::common::Identifier& item);

/// The song's id in the `jukebox_song` registry this server syncs, which a play event carries.
[[nodiscard]] std::optional<std::int32_t> songNetworkId(std::string_view song);

/// parity: `JukeboxSongPlayer.stop` raises its event only when a song was playing.
[[nodiscard]] bool isPlayingAt(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: `JukeboxBlock.HAS_RECORD` plus the disc itself, kept in the block entity.
[[nodiscard]] std::optional<BlockChange> insert(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                                const wire::common::Identifier& disc);

/// Takes the disc back out, which is what right-clicking a playing jukebox does. Answers the change
/// and the disc that popped. With `gameTime`, a song cut short raises `JUKEBOX_STOP_PLAY`.
[[nodiscard]] std::optional<std::pair<BlockChange, wire::common::Identifier>>
eject(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
      std::optional<std::int64_t> gameTime = std::nullopt);

/// What is playing here, or nothing when the jukebox is empty.
[[nodiscard]] std::optional<std::string> songAt(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: `JukeboxSongPlayer.isPlaying`: a disc that has finished is still in the block, so the
/// state cannot say this and the block entity has to.
[[nodiscard]] bool isPlaying(const wire::nbt::NbtCompound& data);

/// One turn of the song. parity: `JukeboxSongPlayer.tick`, which stops the song once it has run its
/// length plus a second. Answers whether it stopped on this turn, because that is when the block
/// stops feeding the redstone around it.
/// With `gameTime`, it also raises `JUKEBOX_PLAY` every twenty ticks and `JUKEBOX_STOP_PLAY` at the end.
[[nodiscard]] bool tick(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int64_t lengthTicks,
                        std::optional<std::int64_t> gameTime = std::nullopt);

/// parity: `JukeboxBlock.getAnalogOutputSignal` into `JukeboxSong.comparatorOutput`, which is looked
/// up from the disc rather than kept beside it: the block entity holds vanilla's `RecordItem` and
/// nothing else, so the number comes from the world's song table.
[[nodiscard]] std::int32_t comparatorOutputAt(const WorldState& world, const wire::nbt::NbtCompound& data);

/// The song numbers, read once each from wherever the jar's data lives.
///
/// The reader is a callback rather than the jar itself: this is the only thing here that needs the
/// operator's own copy of the game, and a test has no jar to hand it.
class SongOutputs {
public:
    using ReadText = std::function<std::optional<std::string>(std::string_view name)>;

    explicit SongOutputs(ReadText readText) : readText_(std::move(readText)) {}

    /// parity: `JukeboxSong.comparatorOutput`, what a comparator reads while this disc plays.
    [[nodiscard]] std::int32_t of(const std::string& song);

    /// parity: `JukeboxSong.lengthInTicks`, how long the song runs before the jukebox goes quiet.
    [[nodiscard]] std::int64_t lengthOf(const std::string& song);

private:
    struct Song {
        std::int32_t comparatorOutput = 0;
        std::int64_t lengthTicks = 0;
    };

    const Song& read(const std::string& song);

    ReadText readText_;
    std::map<std::string, Song, std::less<>> cached_;
};

} // namespace jukeboxes

} // namespace basalt::server::runtime::world
