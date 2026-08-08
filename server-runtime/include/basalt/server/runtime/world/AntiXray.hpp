#pragma once

#include <basalt/server/runtime/config/WorldRules.hpp>
#include <basalt/server/runtime/config/WorldsConfig.hpp>
#include <basalt/server/world/Chunk.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <vector>

namespace basalt::server::runtime::world {

class AntiXray {
public:
    using ChunkLookup = std::function<const server::world::Chunk*(std::int32_t chunkX, std::int32_t chunkZ)>;

    AntiXray(const config::AntiXrayRules& rules, config::WorldDimension dimension);

    /// One entry per section; an empty one is sent as it is.
    [[nodiscard]] std::vector<std::vector<std::int32_t>> obfuscate(const server::world::Chunk& chunk,
                                                                   const ChunkLookup& neighbours) const;

    [[nodiscard]] bool hides(std::int32_t stateId) const noexcept { return hidden_[static_cast<std::size_t>(stateId)]; }

    /// Whether replacing `before` with `after` lets someone see into what lay behind it.
    [[nodiscard]] bool opens(std::int32_t before, std::int32_t after) const noexcept;

    [[nodiscard]] std::int32_t highestRevealedY() const noexcept { return limit_ + radius_ - 1; }

    [[nodiscard]] std::int32_t updateRadius() const noexcept { return radius_; }

    [[nodiscard]] bool usePermission() const noexcept { return usePermission_; }

    /// Blocks within the update radius of a broken one, nearest first.
    [[nodiscard]] const std::vector<std::array<std::int32_t, 3>>& revealOffsets() const noexcept { return offsets_; }

private:
    [[nodiscard]] bool solid(std::int32_t stateId) const noexcept { return solid_[static_cast<std::size_t>(stateId)]; }

    std::int32_t mode_ = 1;
    std::int32_t limit_ = 64;
    std::int32_t radius_ = 2;
    bool usePermission_ = false;
    config::WorldDimension dimension_ = config::WorldDimension::Overworld;
    std::vector<bool> solid_;
    std::vector<bool> hidden_;
    std::vector<bool> eligible_;
    std::vector<std::int32_t> fakes_;
    std::vector<std::array<std::int32_t, 3>> offsets_;
};

} // namespace basalt::server::runtime::world
