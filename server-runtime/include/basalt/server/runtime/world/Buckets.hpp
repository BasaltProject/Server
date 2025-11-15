#pragma once

#include <basalt/server/runtime/world/FluidFlow.hpp>
#include <basalt/wire/protocol/play/BlockPos.hpp>

#include <cstdint>
#include <optional>
#include <vector>

namespace basalt::server::runtime::world {

/// Buckets. parity: `BucketItem.use` plus `Item.getPlayerPOVHitResult`.
///
/// A fluid is not a block target, so the client sends no position and the server traces the look
/// vector itself.
namespace buckets {

[[nodiscard]] const wire::common::Identifier& emptyBucket();

[[nodiscard]] const wire::common::Identifier& waterBucket();

[[nodiscard]] const wire::common::Identifier& lavaBucket();

/// What the world lost or gained, and what the bucket in the hand becomes. The replacement is unset
/// in creative, where the item does not change.
struct Result {
    std::vector<FluidChange> changes;
    std::optional<wire::common::Identifier> replacement;
};

/// What a bucket holds, or nothing if this item is not one.
[[nodiscard]] std::optional<FluidKind> contentOf(const wire::common::Identifier& item);

[[nodiscard]] bool isBucket(const wire::common::Identifier& item);

/// Uses `item` along the look vector: what changed, and what the bucket becomes.
///
/// parity: a full bucket traces past fluids onto the first solid face; an empty one stops at the
/// first source.
[[nodiscard]] std::optional<Result> use(const fluidFlow::Context& context, const wire::common::Identifier& item,
                                        double eyeX, double eyeY, double eyeZ, float yaw, float pitch, bool creative);

/// The same, on the block the client says it hit, which is what a click on a block gives us.
///
/// The client only sends `use_item` when its own ray hits nothing, and a filled bucket traces
/// straight through fluids, so aiming at water with a wall behind it arrives here and not in `use`.
/// Without this, a bucket emptied anywhere near a block did nothing at all.
[[nodiscard]] std::optional<Result> useOn(const fluidFlow::Context& context, const wire::common::Identifier& item,
                                          std::int32_t x, std::int32_t y, std::int32_t z,
                                          wire::protocol::play::Direction face, bool creative);

} // namespace buckets

} // namespace basalt::server::runtime::world
