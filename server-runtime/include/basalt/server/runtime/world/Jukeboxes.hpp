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

/// parity: `JukeboxBlock`.
namespace jukeboxes {

[[nodiscard]] const wire::common::Identifier& block();

/// parity: the disc item's own `jukebox_playable`, which names the song it plays.
[[nodiscard]] std::string songOf(const wire::common::Identifier& disc);

[[nodiscard]] bool isDisc(const wire::common::Identifier& item);

[[nodiscard]] std::optional<std::int32_t> songNetworkId(std::string_view song);

/// parity: `JukeboxSongPlayer.stop` raises its event only when a song was playing.
[[nodiscard]] bool isPlayingAt(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: `JukeboxBlock.HAS_RECORD` plus the disc itself, kept in the block entity.
[[nodiscard]] std::optional<BlockChange> insert(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                                const wire::common::Identifier& disc);

/// With `gameTime`, a song cut short raises `JUKEBOX_STOP_PLAY`.
[[nodiscard]] std::optional<std::pair<BlockChange, wire::common::Identifier>>
eject(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
      std::optional<std::int64_t> gameTime = std::nullopt);

[[nodiscard]] std::optional<std::string> songAt(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// parity: `JukeboxSongPlayer.isPlaying`.
[[nodiscard]] bool isPlaying(const wire::nbt::NbtCompound& data);

/// parity: `JukeboxSongPlayer.tick`.
[[nodiscard]] bool tick(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int64_t lengthTicks,
                        std::optional<std::int64_t> gameTime = std::nullopt);

/// parity: `JukeboxSong.comparatorOutput` is looked up from the disc.
[[nodiscard]] std::int32_t comparatorOutputAt(const WorldState& world, const wire::nbt::NbtCompound& data);

/// The reader is a callback because this is the only thing needing the operator's jar, and a test has none.
class SongOutputs {
public:
    using ReadText = std::function<std::optional<std::string>(std::string_view name)>;

    explicit SongOutputs(ReadText readText) : readText_(std::move(readText)) {}

    /// parity: `JukeboxSong.comparatorOutput`, what a comparator reads while this disc plays.
    [[nodiscard]] std::int32_t of(const std::string& song);

    /// parity: `JukeboxSong.lengthInTicks`.
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
