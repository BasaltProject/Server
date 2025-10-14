#pragma once

#include <cstdint>

namespace basalt::server::world::structure {

/// parity: `MineshaftStructure.Type`. Decides the wood a mineshaft is framed in.
enum class MineshaftType : std::uint8_t { Normal, Mesa };

/// The block states a mineshaft is framed in, which is all its type decides.
/// parity: oak for a normal shaft and dark oak for a badlands one.
class MineshaftBlocks {
public:
    explicit MineshaftBlocks(MineshaftType type);

    [[nodiscard]] std::int32_t log() const noexcept { return log_; }

    [[nodiscard]] std::int32_t planks() const noexcept { return planks_; }

    /// A plain fence post, before the sides a support turns on.
    [[nodiscard]] std::int32_t fence() const noexcept { return fence_; }

    [[nodiscard]] std::int32_t fenceWest() const noexcept { return fenceWest_; }

    [[nodiscard]] std::int32_t fenceEast() const noexcept { return fenceEast_; }

private:
    std::int32_t log_;
    std::int32_t planks_;
    std::int32_t fence_;
    std::int32_t fenceWest_;
    std::int32_t fenceEast_;
};

} // namespace basalt::server::world::structure
