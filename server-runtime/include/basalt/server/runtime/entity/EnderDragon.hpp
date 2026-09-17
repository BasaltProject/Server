#pragma once

#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>

namespace basalt::server::runtime::entity {

/// The dragon of the End.
///
/// parity: `EnderDragon`, whose eight `EnderDragonPart`s are entities of their own. A client numbers
/// them itself, as the eight ids after the dragon's (`recreateFromPacket`), and hits them by those
/// ids, so the server hands the dragon a block of nine.
namespace enderDragon {

inline constexpr std::int32_t kPartCount = 8;

[[nodiscard]] bool isDragon(const wire::common::Identifier& type);

/// How many consecutive entity ids an entity of this type takes.
[[nodiscard]] std::int32_t idSpan(const wire::common::Identifier& type);

} // namespace enderDragon

} // namespace basalt::server::runtime::entity
