#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/VanillaBlocks.hpp>

#include <algorithm>
#include <cstdint>
#include <unordered_map>

namespace basalt::server::registry::blocks {
namespace {

using wire::common::Identifier;

struct Tables {
    Registry<BlockState> registry;
    std::unordered_map<Identifier, BlockHardness> hardnessByBlock;
    std::unordered_map<Identifier, std::int32_t> blockIdsByName;
    std::int32_t stateCount = 0;
};

[[nodiscard]] Tables build() {
    Tables tables;
    for (const auto& [stateId, path] : defaultBlockStatesTable()) {
        Identifier block = Identifier::vanilla(std::string{path});
        tables.registry.registerEntry(stateId, block, BlockState{block, stateId, {}});
    }
    for (const BlockHardnessEntry& entry : blockHardnessTable()) {
        tables.hardnessByBlock.emplace(
            Identifier::vanilla(std::string{entry.path}),
            BlockHardness{entry.destroyTime, entry.requiresCorrectTool, entry.explosionResistance});
    }
    for (const auto& [id, path] : blockIdsTable()) {
        tables.blockIdsByName.emplace(Identifier::vanilla(std::string{path}), id);
    }
    // The last block's states run to the end of the palette, so the count is where they stop.
    const auto& [lastStateId, lastPath] = defaultBlockStatesTable().back();
    const blockStateDefinitions::Definition* last =
        blockStateDefinitions::of(Identifier::vanilla(std::string{lastPath}));
    tables.stateCount = lastStateId + (last == nullptr ? 1 : last->stateCount());
    return tables;
}

[[nodiscard]] const Tables& tables() {
    static const Tables built = build();
    return built;
}

/// The path of a named constant, resolved once. Absent means the table does not have it, which is a
/// table bug rather than something to paper over at the call site.
[[nodiscard]] const BlockState& require(std::string_view path) {
    const BlockState* found = named(path);
    static const BlockState missing{Identifier::vanilla("air"), 0, {}};
    return found == nullptr ? missing : *found;
}

} // namespace

const Registry<BlockState>& registry() { return tables().registry; }

std::optional<std::int32_t> blockIdOf(const Identifier& block) {
    const auto found = tables().blockIdsByName.find(block);
    return found == tables().blockIdsByName.end() ? std::nullopt : std::optional{found->second};
}

const BlockHardness* hardnessOf(const Identifier& block) {
    const auto found = tables().hardnessByBlock.find(block);
    return found == tables().hardnessByBlock.end() ? nullptr : &found->second;
}

std::optional<BlockState> byStateId(std::int32_t stateId) {
    const Identifier* owner = blockStateDefinitions::blockOf(stateId);
    if (owner != nullptr) {
        std::map<std::string, std::string> properties = blockStateDefinitions::propertiesOf(stateId);
        if (!properties.empty()) {
            return BlockState{*owner, stateId, std::move(properties)};
        }
    }
    if (const BlockState* entry = tables().registry.byId(stateId)) {
        return *entry;
    }
    return owner == nullptr ? std::nullopt : std::optional{BlockState{*owner, stateId, {}}};
}

std::optional<std::int32_t> stateIdOf(const Identifier& block, const std::map<std::string, std::string>& properties) {
    const std::optional<std::int32_t> defaultId = tables().registry.idOf(block);
    if (!defaultId.has_value()) {
        return std::nullopt;
    }
    if (properties.empty()) {
        return defaultId;
    }
    return blockStateDefinitions::stateIdOf(block, properties, defaultId);
}

std::int32_t stateCount() { return tables().stateCount; }

bool isAir(std::int32_t stateId) {
    return stateId == air().stateId || stateId == voidAir().stateId || stateId == caveAir().stateId;
}

const BlockState* named(std::string_view path) {
    return tables().registry.byName(Identifier::vanilla(std::string{path}));
}

const BlockState& air() {
    static const BlockState& state = require("air");
    return state;
}

const BlockState& stone() {
    static const BlockState& state = require("stone");
    return state;
}

const BlockState& water() {
    static const BlockState& state = require("water");
    return state;
}

const BlockState& lava() {
    static const BlockState& state = require("lava");
    return state;
}

const BlockState& bedrock() {
    static const BlockState& state = require("bedrock");
    return state;
}

const BlockState& voidAir() {
    static const BlockState& state = require("void_air");
    return state;
}

const BlockState& caveAir() {
    static const BlockState& state = require("cave_air");
    return state;
}

} // namespace basalt::server::registry::blocks
