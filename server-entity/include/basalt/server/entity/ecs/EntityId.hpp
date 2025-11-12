#pragma once

#include <cstdint>

namespace basalt::server::entity::ecs {

/// A dense array index (the low 32 bits) packed with a generation counter (the high 32 bits): when
/// an index is recycled after its entity is destroyed, the generation bumps, so a stale id compares
/// unequal to the new entity occupying that slot instead of silently aliasing it.
struct EntityId {
    std::uint64_t raw = 0;

    [[nodiscard]] static constexpr EntityId of(std::int32_t index, std::int32_t generation) noexcept {
        return EntityId{(static_cast<std::uint64_t>(static_cast<std::uint32_t>(generation)) << 32) |
                        static_cast<std::uint64_t>(static_cast<std::uint32_t>(index))};
    }

    [[nodiscard]] constexpr std::int32_t index() const noexcept {
        return static_cast<std::int32_t>(raw & 0xFFFFFFFFULL);
    }

    [[nodiscard]] constexpr std::int32_t generation() const noexcept { return static_cast<std::int32_t>(raw >> 32); }

    friend constexpr bool operator==(EntityId, EntityId) = default;
};

} // namespace basalt::server::entity::ecs
