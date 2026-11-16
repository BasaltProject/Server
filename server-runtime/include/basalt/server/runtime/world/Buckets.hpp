#pragma once

#include <basalt/server/runtime/world/FluidFlow.hpp>
#include <basalt/wire/protocol/play/BlockPos.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace basalt::server::runtime::world {

namespace buckets {

[[nodiscard]] const wire::common::Identifier& emptyBucket();

[[nodiscard]] const wire::common::Identifier& waterBucket();

[[nodiscard]] const wire::common::Identifier& lavaBucket();

using Gate = std::function<bool(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t faceX, std::int32_t faceY,
                                std::int32_t faceZ)>;

struct Result {
    std::vector<FluidChange> changes;
    std::optional<wire::common::Identifier> replacement;
    /// Where the contents went, which a mob bucket spawns its mob at.
    std::array<std::int32_t, 3> at{};
};

[[nodiscard]] std::optional<FluidKind> contentOf(const wire::common::Identifier& item);

[[nodiscard]] bool isBucket(const wire::common::Identifier& item);

[[nodiscard]] std::optional<Result> use(const fluidFlow::Context& context, const wire::common::Identifier& item,
                                        double eyeX, double eyeY, double eyeZ, float yaw, float pitch, bool creative,
                                        const Gate& gate = {});

[[nodiscard]] std::optional<Result> useOn(const fluidFlow::Context& context, const wire::common::Identifier& item,
                                          std::int32_t x, std::int32_t y, std::int32_t z,
                                          wire::protocol::play::Direction face, bool creative, const Gate& gate = {});

/// parity: `MobBucketItem`, whose water is poured as a bucket's and whose sulfur cube has no fluid at all.
[[nodiscard]] std::optional<Result> useMob(const fluidFlow::Context& context, bool water, double eyeX, double eyeY,
                                           double eyeZ, float yaw, float pitch, bool creative, const Gate& gate = {});

[[nodiscard]] std::optional<Result> useMobOn(const fluidFlow::Context& context, bool water, std::int32_t x,
                                             std::int32_t y, std::int32_t z, wire::protocol::play::Direction face,
                                             bool creative, const Gate& gate = {});

} // namespace buckets

} // namespace basalt::server::runtime::world
