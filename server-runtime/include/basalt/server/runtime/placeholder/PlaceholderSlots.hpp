#pragma once

#include <basalt/server/runtime/config/ServerListConfig.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/wire/common/placeholder/Placeholders.hpp>

#include <optional>
#include <string>

namespace basalt::server::runtime::placeholder {

/// What a placeholder can be resolved against: a logged-in player, or a connection that has sent a
/// handshake and nothing else. A placeholder about coordinates is answerable in the first and not in
/// the second, which is why contexts are typed rather than one bag of nullable fields.
///
/// A slot is its own identity: two slots with the same name are still two different slots.
[[nodiscard]] const wire::common::placeholder::PlaceholderSlot<player::PlayerSession>& playerSlot();

[[nodiscard]] const wire::common::placeholder::PlaceholderSlot<config::PingRequest>& pingSlot();

/// The second player of a relational placeholder: "what does A see about B".
///
/// Nothing Basalt registers reads it yet, and it exists anyway, because a relational placeholder
/// has nowhere to put the other side without it. A plugin comparing two players (their team, who
/// can see whom) needs exactly this slot and cannot add one from outside.
[[nodiscard]] const wire::common::placeholder::PlaceholderSlot<player::PlayerSession>& otherPlayerSlot();

/// A placeholder that exists, is spelled correctly, and cannot be answered yet because the system
/// behind it is not written.
///
/// Registered rather than left out on purpose. Leaving it out reports "no such placeholder", which
/// reads as a typo and sends the operator looking for one; this says what is actually missing. It
/// is the same rule the rest of the project follows for parity gaps: declare the gap, never
/// approximate around it.
[[nodiscard]] wire::common::placeholder::PlaceholderValue notImplemented(std::string reason);

} // namespace basalt::server::runtime::placeholder
