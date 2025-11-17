#pragma once

#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>

namespace basalt::server::runtime::world {

/// The biome tags of the operator's own `server.jar`, expanded through the tags they refer to.
///
/// Never committed: a tag is Mojang's data, so it is read at run time like the rest of it.
class BiomeTagLoader {
public:
    /// Reads one entry of the jar by name, or nothing when the jar has no such entry.
    ///
    /// `ServerJarAssets` lives in `server-assets`, which the runtime core does not depend on, so the
    /// jar arrives as this one function instead.
    using ReadEntry = std::function<std::optional<std::string>(std::string_view)>;

    explicit BiomeTagLoader(ReadEntry readEntry) : readEntry_(std::move(readEntry)) {}

    /// The biome ids in the named tag, or nothing at all when the jar has no such tag.
    [[nodiscard]] const std::set<std::string>& biomesIn(const std::string& name);

private:
    [[nodiscard]] std::set<std::string> expand(const std::string& name, std::set<std::string>& seen) const;

    ReadEntry readEntry_;
    std::map<std::string, std::set<std::string>> cache_;
};

} // namespace basalt::server::runtime::world
