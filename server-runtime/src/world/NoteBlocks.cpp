#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/NoteBlocks.hpp>

#include <charconv>
#include <cstdint>
#include <map>
#include <string>

namespace basalt::server::runtime::world::noteBlocks {
namespace {

using registry::NoteBlockInstrument;

[[nodiscard]] const wire::common::Identifier* blockAt(const WorldState& world, std::int32_t x, std::int32_t y,
                                                      std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    return stateId.has_value() ? registry::blockStateDefinitions::blockOf(*stateId) : nullptr;
}

[[nodiscard]] std::string valueOf(const std::map<std::string, std::string>& properties, const std::string& key) {
    const auto found = properties.find(key);
    return found == properties.end() ? std::string{} : found->second;
}

[[nodiscard]] std::int32_t numberOf(const std::map<std::string, std::string>& properties, const std::string& key) {
    const std::string text = valueOf(properties, key);
    std::int32_t value = 0;
    const std::from_chars_result result = std::from_chars(text.data(), text.data() + text.size(), value);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size() ? value : 0;
}

} // namespace

const wire::common::Identifier& block() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("note_block");
    return instance;
}

const NoteBlockInstrument& instrumentAt(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    static const wire::common::Identifier air = wire::common::Identifier::vanilla("air");
    const wire::common::Identifier* aboveBlock = blockAt(world, x, y + 1, z);
    const NoteBlockInstrument& above = registry::noteBlockInstruments::of(aboveBlock == nullptr ? air : *aboveBlock);
    if (above.worksAbove()) {
        return above;
    }
    const wire::common::Identifier* belowBlock = blockAt(world, x, y - 1, z);
    const NoteBlockInstrument& below = registry::noteBlockInstruments::of(belowBlock == nullptr ? air : *belowBlock);
    // parity: a head *under* a note block is not its instrument, so it falls back to the harp.
    if (below.worksAbove()) {
        return registry::noteBlockInstruments::of(air);
    }
    return below;
}

std::optional<std::int32_t> retune(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                   std::int32_t stateId) {
    const wire::common::Identifier* here = registry::blockStateDefinitions::blockOf(stateId);
    if (here == nullptr || *here != block()) {
        return std::nullopt;
    }
    const NoteBlockInstrument& wanted = instrumentAt(world, x, y, z);
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    if (valueOf(properties, "instrument") == wanted.serializedName()) {
        return std::nullopt;
    }
    const std::optional<std::int32_t> tuned = registry::blockStateDefinitions::stateIdOf(
        block(), {{"instrument", std::string{wanted.serializedName()}}}, stateId);
    if (!tuned.has_value() || !world.setBlockState(x, y, z, *tuned)) {
        return std::nullopt;
    }
    return tuned;
}

std::vector<BlockEffect> playNote(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                  std::int32_t stateId) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const registry::NoteBlockInstrument* named =
        registry::noteBlockInstruments::byName(valueOf(properties, "instrument"));
    static const wire::common::Identifier air = wire::common::Identifier::vanilla("air");
    const NoteBlockInstrument& instrument = named == nullptr ? registry::noteBlockInstruments::of(air) : *named;

    const std::optional<std::int32_t> above = world.blockState(x, y + 1, z);
    if (!above.has_value()) {
        return {};
    }
    if (!instrument.worksAbove() && *above != registry::blocks::air().stateId) {
        return {};
    }
    std::vector<BlockEffect> effects{blockEffect::Event{.b0 = 0, .b1 = 0}};
    // parity: `triggerEvent` gives up on a head whose own sound it cannot find, and we never have
    // one: the swing goes out, the sound does not.
    if (instrument.serializedName() == "custom_head") {
        return effects;
    }
    const float pitch =
        instrument.isTunable() ? registry::noteBlockInstruments::pitchOf(numberOf(properties, "note")) : 1.0F;
    effects.push_back(blockEffect::Sound{.name = std::string{instrument.sound()},
                                         .source = wire::protocol::play::SoundSource::Records,
                                         .volume = kNoteVolume,
                                         .pitch = pitch});
    return effects;
}

std::optional<BlockChange> tune(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return std::nullopt;
    }
    const wire::common::Identifier* here = registry::blockStateDefinitions::blockOf(*stateId);
    if (here == nullptr || *here != block()) {
        return std::nullopt;
    }
    const std::int32_t note = numberOf(registry::blockStateDefinitions::propertiesOf(*stateId), "note") + 1;
    const std::optional<std::int32_t> tuned =
        registry::blockStateDefinitions::stateIdOf(block(), {{"note", std::to_string(note % kNotes)}}, *stateId);
    if (!tuned.has_value() || !world.setBlockState(x, y, z, *tuned)) {
        return std::nullopt;
    }
    return BlockChange{.x = x, .y = y, .z = z, .stateId = *tuned, .effects = playNote(world, x, y, z, *tuned)};
}

std::vector<BlockEffect> strike(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return {};
    }
    const wire::common::Identifier* here = registry::blockStateDefinitions::blockOf(*stateId);
    if (here == nullptr || *here != block()) {
        return {};
    }
    return playNote(world, x, y, z, *stateId);
}

} // namespace basalt::server::runtime::world::noteBlocks
