#pragma once

#include <cstdint>

namespace basalt::server::registry {

/// parity: `net.minecraft.world.level.material.PushReaction`.
enum class PushReaction : std::uint8_t { Normal, Destroy, Block, Ignore, PushOnly };

/// Whether a block state can stay where it is, from the real block classes' own `canSurvive`.
///
/// The generated table holds one rule per distinct answer, and a rule says, per direction, what the
/// neighbour there has to be: any block with a sturdy face, one that supports a centre or a rigid
/// block, or a member of a listed set. A state survives when any one of those is satisfied, which is
/// what a probe of one neighbour at a time measures.
///
/// parity: `BlockBehaviour.canSurvive`, minus the handful of blocks `isModelled` names.
namespace blockSupport {

/// Whether anything has to hold this state up at all.
[[nodiscard]] bool needsSupport(std::int32_t stateId);

/// Whether this build knows the state's rule.
///
/// False for the few whose condition one neighbour at a time cannot see: the multi-face blocks
/// (glow lichen, sculk vein, resin clump), a piston head, a big dripleaf stem, and the two that ask
/// what must *not* be beside them (cactus, chorus flower). They are never broken.
[[nodiscard]] bool isModelled(std::int32_t stateId);

/// Whether the state is water or holds it, which is what "there is water about" is asked of.
[[nodiscard]] bool holdsWater(std::int32_t stateId);

/// What a piston does with a state.
///
/// parity: `BlockState.getPistonPushReaction`. The table lists only the states that are not
/// `Normal`, which is nearly everything: a block nobody named simply moves.
[[nodiscard]] PushReaction pushReaction(std::int32_t stateId);

/// parity: `BlockStateBase.isRedstoneConductor`: what a signal reaches through and what a wire
/// climbs over. Not the same as solid: a slab conducts, a piece of glass does not.
[[nodiscard]] bool conductsRedstone(std::int32_t stateId);

/// parity: `BlockState.isRandomlyTicking`, which is what makes crops grow and ice melt.
[[nodiscard]] bool ticksRandomly(std::int32_t stateId);

/// Whether `stateId` survives with those six neighbours around it.
///
/// `wet` is for the blocks that live in or beside water (sugar cane, seagrass, a small dripleaf):
/// they answer against a second set of directions, and a state that only survives there is kept
/// rather than broken when the caller cannot tell.
[[nodiscard]] bool canSurvive(std::int32_t stateId, std::int32_t down, std::int32_t up, std::int32_t north,
                              std::int32_t south, std::int32_t west, std::int32_t east, bool wet = false);

/// A state's block registry id, which is what every block-set table here is written in.
[[nodiscard]] std::int32_t blockRegistryIdOf(std::int32_t stateId);

} // namespace blockSupport

} // namespace basalt::server::registry
