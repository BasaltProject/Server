#pragma once

#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/common/logging/Logger.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::server::runtime::world {

class RealDialogs {
public:
    using ReadText = std::function<std::optional<std::string>(std::string_view name)>;

    using EntriesUnder = std::function<std::vector<std::string>(std::string_view prefix)>;

    struct Jar {
        ReadText readText;
        EntriesUnder entriesUnder;
    };

    explicit RealDialogs(const Jar& jar, wire::common::Logger logger = wire::common::Logger{"Dialogs"});

    /// Sorted by id, so an entry's index is the id the wire carries.
    [[nodiscard]] const std::vector<std::pair<wire::common::Identifier, wire::nbt::NbtCompound>>&
    entries() const noexcept {
        return entries_;
    }

    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }

    [[nodiscard]] std::vector<wire::common::Identifier> ids() const;

    [[nodiscard]] std::optional<std::int32_t> protocolIdOf(const wire::common::Identifier& id) const;

    [[nodiscard]] bool contains(const wire::common::Identifier& id) const;

private:
    wire::common::Logger logger_;
    std::vector<std::pair<wire::common::Identifier, wire::nbt::NbtCompound>> entries_;
    std::map<wire::common::Identifier, std::int32_t> indices_;
};

} // namespace basalt::server::runtime::world
