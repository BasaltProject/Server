#pragma once

#include <basalt/server/runtime/scoreboard/Scoreboard.hpp>
#include <basalt/server/runtime/scoreboard/Teams.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

namespace basalt::server::runtime::scoreboard {

/// parity: `ScoreboardSaveData.Packed`, the `data` of vanilla's `scoreboard.dat`.
[[nodiscard]] wire::nbt::NbtCompound pack(const Scoreboard& scoreboard, const Teams& teams);

/// Adds what `packed` holds to the scoreboard and the teams, which are expected to be empty.
void unpack(const wire::nbt::NbtCompound& packed, Scoreboard& scoreboard, Teams& teams);

} // namespace basalt::server::runtime::scoreboard
