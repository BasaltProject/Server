#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/BlockSupport.hpp>
#include <basalt/server/registry/VanillaEntries.hpp>
#include <basalt/server/registry/VanillaTables.hpp>
#include <basalt/server/registry/World.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>

namespace basalt::server::registry {
namespace {

using wire::common::Identifier;

[[nodiscard]] Identifier vanilla(std::string_view path) { return Identifier::vanilla(std::string{path}); }

} // namespace

namespace biomes {

const Registry<Biome>& registry() {
    static const Registry<Biome> built = [] {
        Registry<Biome> registry;
        for (const auto& [id, path] : biomeIdsTable()) {
            Identifier name = vanilla(path);
            registry.registerEntry(id, name, Biome{name});
        }
        return registry;
    }();
    return built;
}

const Biome& plains() {
    static const Biome& biome = *registry().byName(Identifier::vanilla("plains"));
    return biome;
}

} // namespace biomes

namespace customStats {

std::optional<std::int32_t> idOf(std::string_view name) {
    const auto found = std::ranges::find(customStatsTable(), name);
    return found == customStatsTable().end()
               ? std::nullopt
               : std::optional{static_cast<std::int32_t>(found - customStatsTable().begin())};
}

std::optional<std::string_view> nameOf(std::int32_t id) {
    if (id < 0 || static_cast<std::size_t>(id) >= customStatsTable().size()) {
        return std::nullopt;
    }
    return customStatsTable()[static_cast<std::size_t>(id)];
}

std::size_t count() { return customStatsTable().size(); }

} // namespace customStats

namespace blockMovement {
namespace {

[[nodiscard]] const BlockMovementEntry* entryOf(std::int32_t stateId) {
    static const std::unordered_map<Identifier, const BlockMovementEntry*> table = [] {
        std::unordered_map<Identifier, const BlockMovementEntry*> built;
        for (const BlockMovementEntry& entry : blockMovementTable()) {
            built.emplace(vanilla(entry.block), &entry);
        }
        return built;
    }();
    const Identifier* block = blockStateDefinitions::blockOf(stateId);
    if (block == nullptr) {
        return nullptr;
    }
    const auto found = table.find(*block);
    return found == table.end() ? nullptr : found->second;
}

} // namespace

float frictionOf(std::int32_t stateId) {
    const BlockMovementEntry* entry = entryOf(stateId);
    return entry == nullptr ? kDefaultFriction : entry->friction;
}

float speedFactorOf(std::int32_t stateId) {
    const BlockMovementEntry* entry = entryOf(stateId);
    return entry == nullptr ? kDefaultSpeedFactor : entry->speedFactor;
}

float jumpFactorOf(std::int32_t stateId) {
    const BlockMovementEntry* entry = entryOf(stateId);
    return entry == nullptr ? kDefaultJumpFactor : entry->jumpFactor;
}

float bouncinessOf(std::int32_t stateId) {
    const BlockMovementEntry* entry = entryOf(stateId);
    return entry == nullptr ? kDefaultBounciness : entry->bounciness;
}

} // namespace blockMovement

namespace noteBlockInstruments {
namespace {

[[nodiscard]] const std::vector<NoteBlockInstrument>& all() {
    static const std::vector<NoteBlockInstrument> built = [] {
        std::vector<NoteBlockInstrument> instruments;
        instruments.reserve(instrumentsTable().size());
        for (const InstrumentEntry& entry : instrumentsTable()) {
            instruments.emplace_back(entry.serializedName, entry.sound, entry.kind);
        }
        return instruments;
    }();
    return built;
}

[[nodiscard]] const std::unordered_map<Identifier, const NoteBlockInstrument*>& byBlock() {
    static const std::unordered_map<Identifier, const NoteBlockInstrument*> built = [] {
        std::unordered_map<Identifier, const NoteBlockInstrument*> table;
        for (const auto& [block, name] : blockInstrumentsTable()) {
            const NoteBlockInstrument* instrument = byName(name);
            if (instrument != nullptr) {
                table.emplace(vanilla(block), instrument);
            }
        }
        return table;
    }();
    return built;
}

} // namespace

const NoteBlockInstrument* byName(std::string_view name) {
    const auto found = std::ranges::find(all(), name, &NoteBlockInstrument::serializedName);
    return found == all().end() ? nullptr : &*found;
}

const NoteBlockInstrument& of(const Identifier& block) {
    static const NoteBlockInstrument& harp = *byName("harp");
    const auto found = byBlock().find(block);
    return found == byBlock().end() ? harp : *found->second;
}

float pitchOf(std::int32_t note) { return static_cast<float>(std::pow(2.0, (note - 12) / 12.0)); }

} // namespace noteBlockInstruments

namespace pointsOfInterest {

std::vector<Identifier> ids() {
    std::vector<Identifier> types;
    for (const auto& [type, block] : poiBlocksTable()) {
        Identifier name = vanilla(type);
        if (std::ranges::find(types, name) == types.end()) {
            types.push_back(std::move(name));
        }
    }
    return types;
}

std::vector<Identifier> blocksOf(const Identifier& type) {
    std::vector<Identifier> blocks;
    for (const auto& [name, block] : poiBlocksTable()) {
        if (type.namespaceName() == Identifier::kVanillaNamespace && name == type.path()) {
            blocks.push_back(vanilla(block));
        }
    }
    return blocks;
}

} // namespace pointsOfInterest

namespace fireOdds {
namespace {

struct Table {
    std::vector<std::int32_t> ignite;
    std::vector<std::int32_t> burn;
};

[[nodiscard]] const Table& table() {
    static const Table built = [] {
        Table table;
        std::int32_t highest = 0;
        for (const FireOddsEntry& entry : fireOddsTable()) {
            highest = std::max(highest, entry.stateId);
        }
        table.ignite.assign(static_cast<std::size_t>(highest) + 1, 0);
        table.burn.assign(static_cast<std::size_t>(highest) + 1, 0);
        for (const FireOddsEntry& entry : fireOddsTable()) {
            table.ignite[static_cast<std::size_t>(entry.stateId)] = entry.igniteOdds;
            table.burn[static_cast<std::size_t>(entry.stateId)] = entry.burnOdds;
        }
        return table;
    }();
    return built;
}

/// The table is written in block registry ids, and a waterlogged block answers zero whatever it is
/// made of.
[[nodiscard]] std::int32_t oddsOf(const std::vector<std::int32_t>& odds, std::int32_t stateId) {
    if (blockSupport::holdsWater(stateId)) {
        return 0;
    }
    const std::int32_t block = blockSupport::blockRegistryIdOf(stateId);
    if (block < 0 || static_cast<std::size_t>(block) >= odds.size()) {
        return 0;
    }
    return odds[static_cast<std::size_t>(block)];
}

} // namespace

std::int32_t igniteOdds(std::int32_t stateId) { return oddsOf(table().ignite, stateId); }

std::int32_t burnOdds(std::int32_t stateId) { return oddsOf(table().burn, stateId); }

} // namespace fireOdds

} // namespace basalt::server::registry
