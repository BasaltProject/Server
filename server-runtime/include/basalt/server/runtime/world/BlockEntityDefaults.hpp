#pragma once

#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <optional>

namespace basalt::server::runtime::world {

/// parity: every `saveAdditional` writes its fields whatever they hold.
namespace blockEntityDefaults {

[[nodiscard]] wire::nbt::NbtCompound of(const wire::common::Identifier& block,
                                        std::optional<std::int32_t> stateId = std::nullopt);

} // namespace blockEntityDefaults

} // namespace basalt::server::runtime::world
