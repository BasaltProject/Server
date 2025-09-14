#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/VanillaBlocks.hpp>

#include <algorithm>
#include <unordered_map>
#include <utility>

namespace basalt::server::registry::blockStateDefinitions {
namespace {

using wire::common::Identifier;

/// The whole table, built once: the definitions by block, and their first state ids sorted so that a
/// state id finds its owner by binary search.
struct Table {
    std::unordered_map<Identifier, Definition> byBlock;
    /// Every block's first state id, sorted.
    std::vector<std::int32_t> starts;
    /// The definition that owns each start, or null for a block with a single state.
    std::vector<const Definition*> owners;
    std::vector<Identifier> ownerBlocks;
};

[[nodiscard]] Table build() {
    Table table;
    for (const std::string_view encoded : blockStateProperties()) {
        const std::size_t firstBar = encoded.find('|');
        const std::size_t secondBar = encoded.find('|', firstBar + 1);
        std::int32_t firstStateId = 0;
        for (const char digit : encoded.substr(0, firstBar)) {
            firstStateId = firstStateId * 10 + (digit - '0');
        }
        Identifier block = Identifier::vanilla(std::string{encoded.substr(firstBar + 1, secondBar - firstBar - 1)});

        std::vector<std::string> names;
        std::vector<std::vector<std::string>> values;
        std::string_view rest = encoded.substr(secondBar + 1);
        while (!rest.empty()) {
            const std::size_t end = rest.find(';');
            const std::string_view property = rest.substr(0, end);
            rest = end == std::string_view::npos ? std::string_view{} : rest.substr(end + 1);
            if (property.empty()) {
                continue;
            }
            const std::size_t equals = property.find('=');
            names.emplace_back(property.substr(0, equals));
            std::vector<std::string> options;
            std::string_view list = property.substr(equals + 1);
            while (!list.empty()) {
                const std::size_t comma = list.find(',');
                options.emplace_back(list.substr(0, comma));
                list = comma == std::string_view::npos ? std::string_view{} : list.substr(comma + 1);
            }
            values.push_back(std::move(options));
        }
        table.byBlock.emplace(block, Definition{block, firstStateId, std::move(names), std::move(values)});
    }

    // A block with a single state is not in the table above, so its own id starts and ends it.
    std::vector<std::pair<std::int32_t, Identifier>> firsts;
    firsts.reserve(defaultBlockStates().size());
    for (const auto& [stateId, path] : defaultBlockStates()) {
        Identifier block = Identifier::vanilla(std::string{path});
        const auto found = table.byBlock.find(block);
        firsts.emplace_back(found == table.byBlock.end() ? stateId : found->second.firstStateId(), std::move(block));
    }
    std::ranges::stable_sort(firsts, {}, &std::pair<std::int32_t, Identifier>::first);
    table.starts.reserve(firsts.size());
    table.owners.reserve(firsts.size());
    table.ownerBlocks.reserve(firsts.size());
    for (auto& [start, block] : firsts) {
        const auto found = table.byBlock.find(block);
        table.starts.push_back(start);
        table.owners.push_back(found == table.byBlock.end() ? nullptr : &found->second);
        table.ownerBlocks.push_back(std::move(block));
    }
    return table;
}

[[nodiscard]] const Table& table() {
    static const Table built = build();
    return built;
}

/// Where `stateId` sits in the sorted starts, or absent when it belongs to no block.
[[nodiscard]] std::optional<std::size_t> indexOf(std::int32_t stateId) {
    const Table& built = table();
    const auto after = std::ranges::upper_bound(built.starts, stateId);
    if (after == built.starts.begin()) {
        return std::nullopt;
    }
    const auto index = static_cast<std::size_t>(after - built.starts.begin() - 1);
    const Definition* definition = built.owners[index];
    const std::int32_t count = definition == nullptr ? 1 : definition->stateCount();
    return stateId < built.starts[index] + count ? std::optional{index} : std::nullopt;
}

} // namespace

Definition::Definition(Identifier block, std::int32_t firstStateId, std::vector<std::string> names,
                       std::vector<std::vector<std::string>> values)
    : block_(std::move(block)), firstStateId_(firstStateId), names_(std::move(names)), values_(std::move(values)) {
    strides_.assign(names_.size(), 1);
    std::int32_t stride = 1;
    for (std::size_t index = names_.size(); index > 0; --index) {
        strides_[index - 1] = stride;
        stride *= static_cast<std::int32_t>(values_[index - 1].size());
    }
}

const std::vector<std::string>& Definition::valuesOf(std::string_view property) const {
    static const std::vector<std::string> none;
    const auto found = std::ranges::find(names_, property);
    return found == names_.end() ? none : values_[static_cast<std::size_t>(found - names_.begin())];
}

std::map<std::string, std::string> Definition::propertiesOf(std::int32_t stateId) const {
    std::map<std::string, std::string> properties;
    const std::int32_t offset = stateId - firstStateId_;
    for (std::size_t index = 0; index < names_.size(); ++index) {
        const std::vector<std::string>& options = values_[index];
        const auto position =
            static_cast<std::size_t>((offset / strides_[index]) % static_cast<std::int32_t>(options.size()));
        properties.emplace(names_[index], options[position]);
    }
    return properties;
}

const Definition* of(const Identifier& block) {
    const auto found = table().byBlock.find(block);
    return found == table().byBlock.end() ? nullptr : &found->second;
}

const Identifier* blockOf(std::int32_t stateId) {
    const std::optional<std::size_t> index = indexOf(stateId);
    return index.has_value() ? &table().ownerBlocks[*index] : nullptr;
}

std::map<std::string, std::string> propertiesOf(std::int32_t stateId) {
    const std::optional<std::size_t> index = indexOf(stateId);
    if (!index.has_value()) {
        return {};
    }
    const Definition* definition = table().owners[*index];
    return definition == nullptr ? std::map<std::string, std::string>{} : definition->propertiesOf(stateId);
}

std::optional<std::int32_t> stateIdOf(const Identifier& block, const std::map<std::string, std::string>& properties,
                                      std::optional<std::int32_t> fromStateId) {
    const Definition* definition = of(block);
    if (definition == nullptr) {
        return properties.empty() ? blocks::registry().idOf(block) : std::nullopt;
    }
    const Identifier* owner = fromStateId.has_value() ? blockOf(*fromStateId) : nullptr;
    const std::map<std::string, std::string> current = owner != nullptr && *owner == block
                                                           ? definition->propertiesOf(*fromStateId)
                                                           : definition->propertiesOf(definition->firstStateId());
    std::int32_t stateId = definition->firstStateId();
    for (std::size_t index = 0; index < definition->names_.size(); ++index) {
        const std::string& name = definition->names_[index];
        const auto asked = properties.find(name);
        const std::string& value = asked == properties.end() ? current.at(name) : asked->second;
        const std::vector<std::string>& options = definition->values_[index];
        const auto position = std::ranges::find(options, value);
        if (position == options.end()) {
            return std::nullopt;
        }
        stateId += static_cast<std::int32_t>(position - options.begin()) * definition->strides_[index];
    }
    for (const auto& [name, unused] : properties) {
        if (std::ranges::find(definition->names_, name) == definition->names_.end()) {
            return std::nullopt;
        }
    }
    return stateId;
}

} // namespace basalt::server::registry::blockStateDefinitions
