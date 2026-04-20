#pragma once

#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>

namespace basalt::server::runtime::world {

class BiomeTagLoader {
public:
    using ReadEntry = std::function<std::optional<std::string>(std::string_view)>;

    explicit BiomeTagLoader(ReadEntry readEntry) : readEntry_(std::move(readEntry)) {}

    [[nodiscard]] const std::set<std::string>& biomesIn(const std::string& name);

private:
    [[nodiscard]] std::set<std::string> expand(const std::string& name, std::set<std::string>& seen) const;

    ReadEntry readEntry_;
    std::map<std::string, std::set<std::string>> cache_;
};

} // namespace basalt::server::runtime::world
