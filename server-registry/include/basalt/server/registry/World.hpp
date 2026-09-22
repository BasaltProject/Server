#pragma once

#include <basalt/server/registry/Registry.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::server::registry {

/// A single biome, identified by its vanilla network id.
struct Biome {
    wire::common::Identifier id;

    friend bool operator==(const Biome&, const Biome&) = default;
};

/// What a note block plays, which is not a property of the note block but of the block under it.
///
/// parity: `NoteBlockInstrument`. The three kinds are what decide the rest: only a base block is
/// tunable, and only the others work *above* the note block instead of below it.
class NoteBlockInstrument {
public:
    enum class Kind : std::uint8_t { BaseBlock, MobHead, Custom };

    [[nodiscard]] std::string_view serializedName() const noexcept { return serializedName_; }

    [[nodiscard]] std::string_view sound() const noexcept { return sound_; }

    /// parity: only a base block answers to the note, so a head always plays at pitch one.
    [[nodiscard]] bool isTunable() const noexcept { return kind_ == Kind::BaseBlock; }

    /// parity: a head sits on top of the note block; everything else has to be under it.
    [[nodiscard]] bool worksAbove() const noexcept { return kind_ != Kind::BaseBlock; }

    NoteBlockInstrument(std::string_view serializedName, std::string_view sound, Kind kind)
        : serializedName_(serializedName), sound_(sound), kind_(kind) {}

    friend bool operator==(const NoteBlockInstrument&, const NoteBlockInstrument&) = default;

private:
    std::string_view serializedName_;
    std::string_view sound_;
    Kind kind_;
};

/// One row of the generated instrument table.
struct InstrumentEntry {
    std::string_view serializedName;
    std::string_view sound;
    NoteBlockInstrument::Kind kind;
};

[[nodiscard]] std::span<const std::pair<std::int32_t, std::string_view>> biomeIdsTable();
[[nodiscard]] std::span<const std::string_view> customStatsTable();
[[nodiscard]] std::span<const InstrumentEntry> instrumentsTable();
[[nodiscard]] std::span<const std::pair<std::string_view, std::string_view>> poiBlocksTable();

/// Every vanilla biome, in `BiomeData.bootstrap` order with the overworld ones first. Saved chunks
/// name their biomes, so an id that moves when a version inserts one breaks nothing.
///
/// Biome network ids are dynamic and synced to each client at login, so only internal consistency
/// between this registry, the synced definitions and chunk palettes is observable on the wire.
namespace biomes {

[[nodiscard]] const Registry<Biome>& registry();

[[nodiscard]] const Biome& plains();

} // namespace biomes

/// The `minecraft:custom_stat` registry, in its own order.
///
/// parity: `Stats`, whose `makeCustomStat` calls fill that registry: the statistics screen reads
/// each counter by id, so the order is protocol and not a detail.
namespace customStats {

[[nodiscard]] std::optional<std::int32_t> idOf(std::string_view name);

[[nodiscard]] std::optional<std::string_view> nameOf(std::int32_t id);

[[nodiscard]] std::size_t count();

} // namespace customStats

/// How a block moves whatever is standing on it.
///
/// parity: `Block.getFriction`, `getSpeedFactor` and `getJumpFactor`. Ice is slippery, soul sand and
/// honey are slow, and honey also kills a jump. Everything else is the default, which is why only
/// the blocks that differ are in the table.
namespace blockMovement {

/// parity: `BlockBehaviour.Properties`' own defaults.
inline constexpr float kDefaultFriction = 0.6F;
inline constexpr float kDefaultSpeedFactor = 1.0F;
inline constexpr float kDefaultJumpFactor = 1.0F;
inline constexpr float kDefaultBounciness = 0.0F;

[[nodiscard]] float frictionOf(std::int32_t stateId);
[[nodiscard]] float speedFactorOf(std::int32_t stateId);
[[nodiscard]] float jumpFactorOf(std::int32_t stateId);

/// parity: `Block.getBounceRestitution`, which is what makes slime and a bed throw back what lands
/// on them.
[[nodiscard]] float bouncinessOf(std::int32_t stateId);

} // namespace blockMovement

namespace noteBlockInstruments {

/// What a note block over `block` plays. Anything not in the table is the harp.
[[nodiscard]] const NoteBlockInstrument& of(const wire::common::Identifier& block);

[[nodiscard]] const NoteBlockInstrument* byName(std::string_view name);

/// parity: `NoteBlock.getPitchFromNote`, an octave either side of the middle note.
[[nodiscard]] float pitchOf(std::int32_t note);

} // namespace noteBlockInstruments

/// The `minecraft:point_of_interest_type` registry: which blocks each type is made of.
///
/// parity: `PoiTypes`' own registrations. Kept as block ids, not block *state* ids: a bed, a
/// cauldron or a lightning rod has dozens of states and vanilla registers every one of them, so
/// matching the block is both shorter and exactly as precise.
namespace pointsOfInterest {

[[nodiscard]] std::vector<wire::common::Identifier> ids();

/// The blocks `type` is made of, empty for a type nobody registered.
[[nodiscard]] std::vector<wire::common::Identifier> blocksOf(const wire::common::Identifier& type);

} // namespace pointsOfInterest

/// How readily each block catches fire and how readily it burns away.
///
/// parity: the two `Object2IntMap`s `FireBlock` fills at bootstrap. A waterlogged block answers zero
/// to both, whatever it is made of.
namespace fireOdds {

/// How likely this block is to catch when there is fire nearby.
[[nodiscard]] std::int32_t igniteOdds(std::int32_t stateId);

/// How likely the fire on it is to consume it.
[[nodiscard]] std::int32_t burnOdds(std::int32_t stateId);

} // namespace fireOdds

} // namespace basalt::server::registry
