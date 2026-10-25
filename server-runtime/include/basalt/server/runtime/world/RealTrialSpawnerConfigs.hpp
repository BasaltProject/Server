#pragma once

#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/common/logging/Logger.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::server::runtime::world {

/// The `trial_spawner` registry: the configs a spawner names by key, read from the data the jar and the packs carry.
class RealTrialSpawnerConfigs {
public:
    using ReadText = std::function<std::optional<std::string>(std::string_view name)>;

    using EntriesUnder = std::function<std::vector<std::string>(std::string_view prefix)>;

    struct Jar {
        ReadText readText;
        EntriesUnder entriesUnder;
    };

    explicit RealTrialSpawnerConfigs(const Jar& jar,
                                     wire::common::Logger logger = wire::common::Logger{"TrialSpawners"});

    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }

    [[nodiscard]] const wire::nbt::NbtCompound* definitionOf(const wire::common::Identifier& id) const;

private:
    wire::common::Logger logger_;
    std::vector<std::pair<wire::common::Identifier, wire::nbt::NbtCompound>> entries_;
};

} // namespace basalt::server::runtime::world
