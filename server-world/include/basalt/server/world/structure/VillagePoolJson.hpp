#pragma once

#include <basalt/server/world/structure/StructureTemplatePool.hpp>

#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <string_view>

namespace basalt::server::world::structure {

namespace villagePoolJson {

struct ParseError {
    std::string message;
};

using TagResolver = std::function<std::optional<std::set<std::int32_t>>(std::string_view)>;

using ProcessorListResolver = std::function<std::optional<std::string>(std::string_view)>;

[[nodiscard]] std::expected<StructureTemplatePool, ParseError>
parsePool(std::string_view json, const ProcessorListResolver& resolveProcessors, const TagResolver& resolveTag = {});

[[nodiscard]] std::expected<StructureProcessorList, ParseError> parseProcessorList(std::string_view json,
                                                                                   const TagResolver& resolveTag = {});

} // namespace villagePoolJson

} // namespace basalt::server::world::structure
