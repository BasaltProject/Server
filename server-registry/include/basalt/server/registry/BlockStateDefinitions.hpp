#pragma once

#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::registry {

/// Every block's properties and how its state ids run through them.
///
/// The ids are the nested loops of the properties, outermost first, so a state id and a set of
/// values convert into each other with arithmetic instead of a table per family.
///
/// parity: `StateDefinition`, which builds the same product in the same order.
namespace blockStateDefinitions {

/// One block: where its states start, and each property with the values it cycles through.
class Definition {
public:
    Definition(wire::common::Identifier block, std::int32_t firstStateId, std::vector<std::string> names,
               std::vector<std::vector<std::string>> values);

    [[nodiscard]] const wire::common::Identifier& block() const noexcept { return block_; }

    [[nodiscard]] std::int32_t firstStateId() const noexcept { return firstStateId_; }

    [[nodiscard]] std::int32_t stateCount() const noexcept {
        return names_.empty() ? 1 : strides_.front() * static_cast<std::int32_t>(values_.front().size());
    }

    [[nodiscard]] const std::vector<std::string>& propertyNames() const noexcept { return names_; }

    [[nodiscard]] const std::vector<std::string>& valuesOf(std::string_view property) const;

    [[nodiscard]] std::map<std::string, std::string> propertiesOf(std::int32_t stateId) const;

private:
    friend std::optional<std::int32_t>
    stateIdOf(const wire::common::Identifier&, const std::map<std::string, std::string>&, std::optional<std::int32_t>);

    wire::common::Identifier block_;
    std::int32_t firstStateId_;
    std::vector<std::string> names_;
    std::vector<std::vector<std::string>> values_;
    std::vector<std::int32_t> strides_;
};

[[nodiscard]] const Definition* of(const wire::common::Identifier& block);

/// Which block owns `stateId`, properties or not.
[[nodiscard]] const wire::common::Identifier* blockOf(std::int32_t stateId);

/// The property values of `stateId`, empty for a block that has none.
[[nodiscard]] std::map<std::string, std::string> propertiesOf(std::int32_t stateId);

/// The state id of `block` with `properties` applied, the rest kept from `fromStateId` when it
/// belongs to the same block and from the block's first state otherwise.
///
/// An unknown property or value is refused rather than ignored: silently placing a block facing the
/// wrong way is worse than not placing it.
[[nodiscard]] std::optional<std::int32_t> stateIdOf(const wire::common::Identifier& block,
                                                    const std::map<std::string, std::string>& properties,
                                                    std::optional<std::int32_t> fromStateId = std::nullopt);

} // namespace blockStateDefinitions

} // namespace basalt::server::registry
