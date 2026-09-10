#pragma once

#include <basalt/wire/common/Identifier.hpp>

namespace basalt::server::runtime::entity::blockTriggers {

/// Whether a plate, a tripwire or anything that counts bodies should behave as if this entity were
/// not there. parity: `Entity.isIgnoringBlockTriggers`, false for all but five kinds. An armour
/// stand joins them only while `Marker` is on, which no type name can tell, so the caller asks.
[[nodiscard]] bool ignored(const wire::common::Identifier& type);

} // namespace basalt::server::runtime::entity::blockTriggers
