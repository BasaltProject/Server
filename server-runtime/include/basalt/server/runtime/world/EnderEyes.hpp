#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace basalt::server::runtime::player {
struct BreakContext;
class PlayerRegistry;
class PlayerSession;
} // namespace basalt::server::runtime::player

namespace basalt::server::runtime::entity {
class ServerEntity;

namespace enderEyes {

/// parity: `EyeOfEnder.target`, `life` and `surviveAfterDeath`, none of which is saved.
struct EyeState {
    std::optional<std::array<double, 3>> target;
    std::int32_t life = 0;
    bool surviveAfterDeath = false;
};

} // namespace enderEyes
} // namespace basalt::server::runtime::entity

namespace basalt::server::runtime::world {

class LoadedWorld;

/// Thrown eyes of ender. parity: `EnderEyeItem.use` and `EyeOfEnder`.
namespace enderEyes {

[[nodiscard]] const wire::common::Identifier& type();

[[nodiscard]] bool isEye(const entity::ServerEntity& entity);

/// parity: `EyeOfEnder.signalTo`: straight at a feature within twelve blocks, else twelve blocks
/// its way and eight up.
void signalTo(entity::ServerEntity& eye, double x, double y, double z, random::RandomSource& random);

/// What a tick left of the eye.
enum class Fate : std::uint8_t { Flying, Dropped, Shattered };

/// parity: `EyeOfEnder.tick`'s movement and life; the caller plays out the fate.
[[nodiscard]] Fate step(entity::ServerEntity& eye);

/// Throws the eye in that slot at the nearest stronghold. False when the item is not an eye or
/// the use passes on to a frame the player is looking at.
bool throwFor(const player::BreakContext& context, player::PlayerSession& player, LoadedWorld& world,
              std::int32_t slot);

/// One tick of a flying eye, sound, drop and shatter included.
void tick(LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers, player::PlayerRegistry& registry,
          entity::ServerEntity& eye, std::int64_t seed);

} // namespace enderEyes

} // namespace basalt::server::runtime::world
