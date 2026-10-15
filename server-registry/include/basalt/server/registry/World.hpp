#pragma once

#include <basalt/server/registry/Registry.hpp>
#include <basalt/server/registry/VanillaEntries.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::server::registry {
struct Biome {
    wire::common::Identifier id;

    friend bool operator==(const Biome&, const Biome&) = default;
};

// parity: `NoteBlockInstrument`.
class NoteBlockInstrument {
public:
    enum class Kind : std::uint8_t { BaseBlock, MobHead, Custom };

    [[nodiscard]] std::string_view serializedName() const noexcept { return serializedName_; }

    [[nodiscard]] std::string_view sound() const noexcept { return sound_; }

    // parity: only a base block answers to the note, so a head always plays at pitch one.
    [[nodiscard]] bool isTunable() const noexcept { return kind_ == Kind::BaseBlock; }

    // parity: a head sits on top of the note block; everything else has to be under it.
    [[nodiscard]] bool worksAbove() const noexcept { return kind_ != Kind::BaseBlock; }

    NoteBlockInstrument(std::string_view serializedName, std::string_view sound, Kind kind)
        : serializedName_(serializedName), sound_(sound), kind_(kind) {}

    friend bool operator==(const NoteBlockInstrument&, const NoteBlockInstrument&) = default;

private:
    std::string_view serializedName_;
    std::string_view sound_;
    Kind kind_;
};

struct InstrumentEntry {
    std::string_view serializedName;
    std::string_view sound;
    NoteBlockInstrument::Kind kind;
};

[[nodiscard]] std::span<const std::pair<std::int32_t, std::string_view>> biomeIdsTable();
[[nodiscard]] std::span<const std::string_view> customStatsTable();
[[nodiscard]] std::span<const InstrumentEntry> instrumentsTable();
[[nodiscard]] std::span<const std::pair<std::string_view, std::string_view>> poiBlocksTable();

namespace biomes {
[[nodiscard]] const Registry<Biome>& registry();

[[nodiscard]] const Biome& plains();
} // namespace biomes

// parity: `Stats`.
namespace customStats {
[[nodiscard]] std::optional<std::int32_t> idOf(std::string_view name);

[[nodiscard]] std::optional<std::string_view> nameOf(std::int32_t id);

[[nodiscard]] std::size_t count();
} // namespace customStats

// parity: `Block.getFriction`, `getSpeedFactor` and `getJumpFactor`.
namespace blockMovement {
// parity: `BlockBehaviour.Properties`' own defaults.
inline constexpr float kDefaultFriction = 0.6F;
inline constexpr float kDefaultSpeedFactor = 1.0F;
inline constexpr float kDefaultJumpFactor = 1.0F;
inline constexpr float kDefaultBounciness = 0.0F;

[[nodiscard]] float frictionOf(std::int32_t stateId);
[[nodiscard]] float speedFactorOf(std::int32_t stateId);
[[nodiscard]] float jumpFactorOf(std::int32_t stateId);

// parity: `Block.getBounceRestitution`.
[[nodiscard]] float bouncinessOf(std::int32_t stateId);
} // namespace blockMovement

namespace blockSounds {
// parity: `BlockState.getSoundType`, stone's for anything the table does not name.
[[nodiscard]] const BlockSoundTypeEntry& of(std::int32_t stateId);
} // namespace blockSounds

namespace noteBlockInstruments {
[[nodiscard]] const NoteBlockInstrument& of(const wire::common::Identifier& block);

[[nodiscard]] const NoteBlockInstrument* byName(std::string_view name);

// parity: `NoteBlock.getPitchFromNote`, an octave either side of the middle note.
[[nodiscard]] float pitchOf(std::int32_t note);
} // namespace noteBlockInstruments

// parity: `PoiTypes`' registrations.
namespace pointsOfInterest {
[[nodiscard]] std::vector<wire::common::Identifier> ids();

[[nodiscard]] std::vector<wire::common::Identifier> blocksOf(const wire::common::Identifier& type);
} // namespace pointsOfInterest

// parity: the two `Object2IntMap`s `FireBlock` fills at bootstrap.
namespace fireOdds {
[[nodiscard]] std::int32_t igniteOdds(std::int32_t stateId);

[[nodiscard]] std::int32_t burnOdds(std::int32_t stateId);
} // namespace fireOdds
} // namespace basalt::server::registry
