#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/BlockSupport.hpp>
#include <basalt/server/registry/VanillaBlocks.hpp>
#include <basalt/server/registry/VanillaTables.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <map>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace basalt::server::registry::blockSupport {
namespace {
constexpr std::int32_t kNone = 0;
constexpr std::int32_t kSturdy = 1;
constexpr std::int32_t kCentre = 2;
constexpr std::int32_t kRigid = 3;
constexpr std::int32_t kFirstSet = 4;

constexpr std::int32_t kAlways = 0;
constexpr std::int32_t kUnknown = 1;
constexpr std::int32_t kFirstRule = 2;

constexpr std::size_t kDirections = 6;
constexpr std::string_view kIndexAlphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

[[nodiscard]] std::vector<std::uint8_t> decodeBase64(std::string_view text) {
    while (!text.empty() && text.back() == '=') {
        text.remove_suffix(1);
    }
    std::vector<std::uint8_t> bytes;
    bytes.reserve(text.size() * 6 / 8);
    std::uint32_t buffer = 0;
    int bits = 0;
    for (const char character : text) {
        buffer = (buffer << 6) | static_cast<std::uint32_t>(kIndexAlphabet.find(character));
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            bytes.push_back(static_cast<std::uint8_t>((buffer >> bits) & 0xFF));
        }
    }
    return bytes;
}

[[nodiscard]] PushReaction reactionOf(std::string_view name) {
    if (name == "DESTROY") {
        return PushReaction::Destroy;
    }
    if (name == "BLOCK") {
        return PushReaction::Block;
    }
    if (name == "IGNORE") {
        return PushReaction::Ignore;
    }
    if (name == "PUSH_ONLY") {
        return PushReaction::PushOnly;
    }
    return PushReaction::Normal;
}

struct Table {
    std::vector<std::array<std::int32_t, kDirections * 2>> rules;
    std::vector<std::vector<std::uint8_t>> sets;
    std::vector<std::int32_t> byState;
    std::vector<bool> randomTicking;
    std::vector<bool> redstoneConductors;
    std::unordered_map<std::int32_t, PushReaction> pushReactions;
    std::vector<std::int32_t> blockOfState;
    std::vector<bool> waterlike;
};

[[nodiscard]] const Table& table() {
    static const Table built = [] {
        Table table;
        table.rules.reserve(supportRulesTable().size());
        for (const std::string_view encoded : supportRulesTable()) {
            std::array<std::int32_t, kDirections * 2> fields{};
            std::string_view rest = encoded;
            for (std::size_t field = 0; field < fields.size(); ++field) {
                const std::size_t bar = rest.find('|');
                const std::string_view token = rest.substr(0, bar);
                rest = bar == std::string_view::npos ? std::string_view{} : rest.substr(bar + 1);
                if (token.empty()) {
                    fields[field] = kNone;
                } else if (token == "*") {
                    fields[field] = kSturdy;
                } else if (token == "c") {
                    fields[field] = kCentre;
                } else if (token == "r") {
                    fields[field] = kRigid;
                } else {
                    std::int32_t set = 0;
                    const std::string_view digits = token.substr(1);
                    std::from_chars(digits.data(), digits.data() + digits.size(), set);
                    fields[field] = kFirstSet + set;
                }
            }
            table.rules.push_back(fields);
        }
        table.sets.reserve(supportSetsTable().size());
        for (const std::string_view set : supportSetsTable()) {
            table.sets.push_back(decodeBase64(set));
        }

        const std::string_view index = blockSupportIndexTable();
        table.byState.reserve(index.size() / 2);
        for (std::size_t state = 0; state * 2 + 1 < index.size(); ++state) {
            const auto high = static_cast<std::int32_t>(kIndexAlphabet.find(index[state * 2]));
            const auto low = static_cast<std::int32_t>(kIndexAlphabet.find(index[state * 2 + 1]));
            table.byState.push_back(high * static_cast<std::int32_t>(kIndexAlphabet.size()) + low);
        }

        std::int32_t highestTicking = 0;
        for (const std::int32_t state : randomTickingStatesTable()) {
            highestTicking = std::max(highestTicking, state);
        }
        table.randomTicking.assign(static_cast<std::size_t>(highestTicking) + 1, false);
        for (const std::int32_t state : randomTickingStatesTable()) {
            table.randomTicking[static_cast<std::size_t>(state)] = true;
        }

        std::int32_t highestConductor = 0;
        for (const std::int32_t state : redstoneConductorStatesTable()) {
            highestConductor = std::max(highestConductor, state);
        }
        table.redstoneConductors.assign(static_cast<std::size_t>(highestConductor) + 1, false);
        for (const std::int32_t state : redstoneConductorStatesTable()) {
            table.redstoneConductors[static_cast<std::size_t>(state)] = true;
        }

        for (const auto& [state, reaction] : pushReactionsTable()) {
            table.pushReactions.emplace(state, reactionOf(reaction));
        }

        std::unordered_map<std::string_view, std::int32_t> blockIds;
        for (const auto& [id, path] : blockIdsTable()) {
            blockIds.emplace(path, id);
        }
        table.blockOfState.assign(table.byState.size(), -1);
        table.waterlike.assign(table.byState.size(), false);
        for (std::size_t state = 0; state < table.byState.size(); ++state) {
            const wire::common::Identifier* block = blockStateDefinitions::blockOf(static_cast<std::int32_t>(state));
            if (block == nullptr) {
                continue;
            }
            const auto id = blockIds.find(block->path());
            table.blockOfState[state] = id == blockIds.end() ? -1 : id->second;
            if (block->path() == "water") {
                table.waterlike[state] = true;
                continue;
            }
            const blockStateDefinitions::Definition* definition = blockStateDefinitions::of(*block);
            if (definition == nullptr) {
                continue;
            }
            const std::vector<std::string>& names = definition->propertyNames();
            if (std::ranges::find(names, "waterlogged") == names.end()) {
                continue;
            }
            const std::map<std::string, std::string> properties =
                blockStateDefinitions::propertiesOf(static_cast<std::int32_t>(state));
            const auto waterlogged = properties.find("waterlogged");
            table.waterlike[state] = waterlogged != properties.end() && waterlogged->second == "true";
        }
        return table;
    }();
    return built;
}

[[nodiscard]] std::int32_t ruleOf(std::int32_t stateId) {
    const Table& built = table();
    if (stateId < 0 || static_cast<std::size_t>(stateId) >= built.byState.size()) {
        return kAlways;
    }
    return built.byState[static_cast<std::size_t>(stateId)];
}

[[nodiscard]] bool contains(const std::vector<std::uint8_t>& set, std::int32_t blockId) {
    if (blockId < 0) {
        return false;
    }
    const auto index = static_cast<std::size_t>(blockId) / 8;
    return index < set.size() && ((set[index] >> (blockId % 8)) & 1) != 0;
}

[[nodiscard]] bool satisfies(std::int32_t requirement, std::int32_t neighbour, BlockFace towards) {
    switch (requirement) {
    case kSturdy:
        return blockShapes::isFaceSturdy(neighbour, towards);
    case kCentre:
        return blockShapes::supportsCentre(neighbour, towards);
    case kRigid:
        return blockShapes::supportsRigid(neighbour, towards);
    default:
        return contains(table().sets[static_cast<std::size_t>(requirement - kFirstSet)], blockRegistryIdOf(neighbour));
    }
}
} // namespace

bool needsSupport(std::int32_t stateId) { return ruleOf(stateId) != kAlways; }

bool isModelled(std::int32_t stateId) { return ruleOf(stateId) != kUnknown; }

bool holdsWater(std::int32_t stateId) {
    const Table& built = table();
    return stateId >= 0 && static_cast<std::size_t>(stateId) < built.waterlike.size() &&
           built.waterlike[static_cast<std::size_t>(stateId)];
}

PushReaction pushReaction(std::int32_t stateId) {
    const auto found = table().pushReactions.find(stateId);
    return found == table().pushReactions.end() ? PushReaction::Normal : found->second;
}

bool conductsRedstone(std::int32_t stateId) {
    const Table& built = table();
    return stateId >= 0 && static_cast<std::size_t>(stateId) < built.redstoneConductors.size() &&
           built.redstoneConductors[static_cast<std::size_t>(stateId)];
}

bool ticksRandomly(std::int32_t stateId) {
    const Table& built = table();
    return stateId >= 0 && static_cast<std::size_t>(stateId) < built.randomTicking.size() &&
           built.randomTicking[static_cast<std::size_t>(stateId)];
}

bool canSurvive(std::int32_t stateId, std::int32_t down, std::int32_t up, std::int32_t north, std::int32_t south,
                std::int32_t west, std::int32_t east, bool wet) {
    const std::int32_t rule = ruleOf(stateId);
    if (rule == kAlways || rule == kUnknown) {
        return true;
    }
    const std::array<std::int32_t, kDirections * 2>& fields =
        table().rules[static_cast<std::size_t>(rule - kFirstRule)];
    const std::array<std::int32_t, kDirections> neighbours = {down, up, north, south, west, east};
    const std::size_t slots = wet ? kDirections * 2 : kDirections;
    for (std::size_t slot = 0; slot < slots; ++slot) {
        const std::int32_t requirement = fields[slot];
        if (requirement == kNone) {
            continue;
        }
        const auto face = static_cast<BlockFace>(slot % kDirections);
        if (satisfies(requirement, neighbours[slot % kDirections], opposite(face))) {
            return true;
        }
    }
    return false;
}

std::int32_t blockRegistryIdOf(std::int32_t stateId) {
    const Table& built = table();
    if (stateId < 0 || static_cast<std::size_t>(stateId) >= built.blockOfState.size()) {
        return -1;
    }
    return built.blockOfState[static_cast<std::size_t>(stateId)];
}
} // namespace basalt::server::registry::blockSupport
