#pragma once

#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

namespace basalt::server::runtime::world {

/// What a block entity holds the moment it is put down, before anything has happened to it.
///
/// parity: every `saveAdditional` writes its fields whatever they hold, so one that has never ticked
/// is not an empty bag. Without this a world of ours opens in vanilla with block entities missing
/// half their fields. Only the types whose save writes unconditionally are here.
namespace blockEntityDefaults {

[[nodiscard]] wire::nbt::NbtCompound of(const wire::common::Identifier& block);

} // namespace blockEntityDefaults

} // namespace basalt::server::runtime::world
