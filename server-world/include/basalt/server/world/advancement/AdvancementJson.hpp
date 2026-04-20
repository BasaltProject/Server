#pragma once

#include <basalt/server/world/advancement/Advancement.hpp>

#include <expected>
#include <string>
#include <string_view>

namespace basalt::server::world::advancement {

struct AdvancementParseError {
    std::string message;
};

namespace advancementJson {

[[nodiscard]] std::expected<Advancement, AdvancementParseError> parse(std::string_view json);

} // namespace advancementJson

} // namespace basalt::server::world::advancement
