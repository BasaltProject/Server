#pragma once

#include <basalt/server/registry/Tags.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/common/logging/Logger.hpp>

#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::server::runtime::datapack {

using EntriesUnder = std::function<std::vector<std::string>(std::string_view prefix)>;
using ReadText = std::function<std::optional<std::string>(std::string_view name)>;

/// Every `data/<namespace>/<directory>/<path><extension>` entry, as its id and entry name, over
/// every namespace. parity: `FileToIdConverter.listMatchingResources`, sorted by id.
[[nodiscard]] std::vector<std::pair<wire::common::Identifier, std::string>>
dataFiles(const EntriesUnder& entriesUnder, std::string_view directory, std::string_view extension = ".json");

/// Every tag file of one registry (`item`, `worldgen/biome`...), read and parsed, by tag id. A file
/// that does not parse is logged and skipped. parity: `TagLoader.load`.
[[nodiscard]] std::map<wire::common::Identifier, registry::tags::TagFile> tagFiles(const EntriesUnder& entriesUnder,
                                                                                   const ReadText& readText,
                                                                                   std::string_view registry,
                                                                                   const wire::common::Logger& logger);

/// The same, built: a tag with a missing required reference is logged and left out.
[[nodiscard]] std::map<wire::common::Identifier, std::set<wire::common::Identifier>>
resolveTags(const EntriesUnder& entriesUnder, const ReadText& readText, std::string_view registry,
            const registry::tags::ElementExists& exists, const wire::common::Logger& logger);

/// The same, each tag in file order.
[[nodiscard]] std::map<wire::common::Identifier, std::vector<wire::common::Identifier>>
resolveTagsOrdered(const EntriesUnder& entriesUnder, const ReadText& readText, std::string_view registry,
                   const registry::tags::ElementExists& exists, const wire::common::Logger& logger);

} // namespace basalt::server::runtime::datapack
