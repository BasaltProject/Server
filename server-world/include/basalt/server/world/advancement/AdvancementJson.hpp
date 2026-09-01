#pragma once

#include <basalt/server/world/advancement/Advancement.hpp>

#include <expected>
#include <string>
#include <string_view>

namespace basalt::server::world::advancement {

/// Why an advancement did not parse: a file missing what every advancement has, not malformed JSON.
struct AdvancementParseError {
    std::string message;
};

/// Parses the real vanilla advancement JSON schema. A missing `requirements` defaults to one group
/// per criterion, an implicit AND of everything, mirroring `AdvancementRequirements.allOf`.
namespace advancementJson {

[[nodiscard]] std::expected<Advancement, AdvancementParseError> parse(std::string_view json);

} // namespace advancementJson

} // namespace basalt::server::world::advancement
