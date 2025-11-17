#pragma once

#include <basalt/server/world/structure/StructureTemplate.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/common/logging/Logger.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

/// Structure files out of the operator's own `server.jar`, by id.
///
/// The same files world generation already reads for villages and bastions; this is what lets
/// `/place template` put one down on demand. Read lazily and kept, because there are hundreds of
/// them and a server that never runs the command should not pay for any.
class StructureTemplates {
public:
    /// One jar entry as bytes, or nothing when the jar has no such entry.
    using ReadEntry = std::function<std::optional<std::vector<std::uint8_t>>(std::string_view name)>;

    /// Every id under a data directory with a given extension, which is a walk of the jar.
    using DataIds =
        std::function<std::vector<wire::common::Identifier>(std::string_view directory, std::string_view extension)>;

    /// The jar arrives as callbacks rather than as `ServerJarAssets`: this needs the operator's own
    /// copy of the game, and a test has no jar to hand it.
    struct Jar {
        ReadEntry readEntry;
        DataIds dataIds;
    };

    explicit StructureTemplates(Jar jar, wire::common::Logger logger = wire::common::Logger{"Structures"})
        : jar_(std::move(jar)), logger_(std::move(logger)) {}

    /// The named structure, or nothing when the jar ships no such file.
    ///
    /// The answer is kept either way, so a name that is not there is only looked for once.
    [[nodiscard]] const server::world::structure::StructureTemplate* find(const wire::common::Identifier& id);

    /// Every structure file the jar ships. parity: `PlaceCommand.SUGGEST_TEMPLATES`.
    [[nodiscard]] const std::vector<wire::common::Identifier>& templateIds();

    /// Every template pool the jar ships, which is what `/place jigsaw` grows one of.
    [[nodiscard]] const std::vector<wire::common::Identifier>& poolIds();

private:
    [[nodiscard]] const std::vector<wire::common::Identifier>& idsUnder(const std::string& directory,
                                                                        std::string_view extension);

    Jar jar_;
    wire::common::Logger logger_;
    std::mutex mutex_;
    /// A map rather than a vector: a pointer handed out has to survive the next read.
    std::map<wire::common::Identifier, std::optional<server::world::structure::StructureTemplate>> cache_;
    std::map<std::string, std::vector<wire::common::Identifier>> listings_;
};

} // namespace basalt::server::runtime::world
