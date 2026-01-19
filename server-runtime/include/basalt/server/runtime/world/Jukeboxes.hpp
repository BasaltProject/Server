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

/// The block that plays a disc.
///
/// parity: `JukeboxBlock` over `JukeboxBlockEntity`. A disc goes in, the block says so, and while it
/// plays the block gives off a full signal and a comparator reads the song's own number: eleven for
/// `11`, thirteen for `13`, and so on. Those numbers are data
/// (`data/minecraft/jukebox_song/<name>.json`), so they are read from the operator's own jar.
namespace jukeboxes {

[[nodiscard]] const wire::common::Identifier& block();

/// parity: the disc item's own `jukebox_playable`, which names the song it plays.
[[nodiscard]] std::string songOf(const wire::common::Identifier& disc);

/// Whether this item is a disc a jukebox takes.
[[nodiscard]] bool isDisc(const wire::common::Identifier& item);

/// parity: `JukeboxBlock.HAS_RECORD` plus the disc itself, kept in the block entity.
[[nodiscard]] std::optional<BlockChange> insert(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                                const wire::common::Identifier& disc,
                                                std::int32_t comparatorOutput = 0);

/// Takes the disc back out, which is what right-clicking a playing jukebox does. Answers the change
/// and the disc that popped.
[[nodiscard]] std::optional<std::pair<BlockChange, wire::common::Identifier>> eject(WorldState& world, std::int32_t x,
                                                                                    std::int32_t y, std::int32_t z);

/// What is playing here, or nothing when the jukebox is empty.
[[nodiscard]] std::optional<std::string> songAt(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: `JukeboxSongPlayer.isPlaying`: a disc that has finished is still in the block, so the
/// state cannot say this and the block entity has to.
[[nodiscard]] bool isPlaying(const wire::nbt::NbtCompound& data);

/// One turn of the song. parity: `JukeboxSongPlayer.tick`, which stops the song once it has run its
/// length plus a second. Answers whether it stopped on this turn, because that is when the block
/// stops feeding the redstone around it.
[[nodiscard]] bool tick(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int64_t lengthTicks);

/// parity: `JukeboxBlock.getAnalogOutputSignal`, the playing song's own number.
[[nodiscard]] std::int32_t comparatorOutputAt(const wire::nbt::NbtCompound& data);

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
