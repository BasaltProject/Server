#pragma once

#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>

namespace basalt::server::runtime::player {

class PlayerSession;

[[nodiscard]] bool onCooldownFor(PlayerSession& session, const wire::common::Identifier& item, std::int64_t gameTime);

void startCooldownFor(PlayerSession& session, const wire::common::Identifier& item, std::int64_t gameTime);

void startCooldownFor(PlayerSession& session, const wire::common::Identifier& item, std::int64_t gameTime,
                      std::int32_t ticks);

} // namespace basalt::server::runtime::player
