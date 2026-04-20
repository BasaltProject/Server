#pragma once

#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/common/logging/Logger.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::server::runtime::world {

/// A timeline maps clock ticks to environment attributes; without one `/time set night` leaves the sky where it was.
class RealTimelines {
public:
    using ReadText = std::function<std::optional<std::string>(std::string_view name)>;

    using EntriesUnder = std::function<std::vector<std::string>(std::string_view prefix)>;

    struct Jar {
        ReadText readText;
        EntriesUnder entriesUnder;
    };

    explicit RealTimelines(const Jar& jar, wire::common::Logger logger = wire::common::Logger{"Timelines"});

    [[nodiscard]] const std::vector<std::pair<wire::common::Identifier, wire::nbt::NbtCompound>>&
    entries() const noexcept {
        return entries_;
    }

    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }

    [[nodiscard]] std::vector<wire::common::Identifier> ids() const;

    [[nodiscard]] const wire::nbt::NbtCompound* definitionOf(const wire::common::Identifier& id) const;

private:
    wire::common::Logger logger_;
    std::vector<std::pair<wire::common::Identifier, wire::nbt::NbtCompound>> entries_;
};

} // namespace basalt::server::runtime::world
