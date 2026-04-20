#pragma once

#include <basalt/wire/common/Identifier.hpp>

namespace basalt::server::runtime::entity::blockTriggers {

/// parity: `Entity.isIgnoringBlockTriggers`, false for all but five kinds.
[[nodiscard]] bool ignored(const wire::common::Identifier& type);

} // namespace basalt::server::runtime::entity::blockTriggers
