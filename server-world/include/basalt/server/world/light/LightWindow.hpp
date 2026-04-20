#pragma once

#include <basalt/server/world/Chunk.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>

namespace basalt::server::world::light {

class LightWindow {
public:
    static constexpr std::int32_t kWidth = 48;

    using Neighbours = std::function<const Chunk*(std::int32_t chunkX, std::int32_t chunkZ)>;

    LightWindow(const Chunk& centre, const Neighbours& neighbours);

    [[nodiscard]] const Chunk& centre() const noexcept { return *chunks_[4]; }

    [[nodiscard]] std::int32_t minY() const noexcept { return minY_; }

    [[nodiscard]] std::int32_t maxY() const noexcept { return maxY_; }

    [[nodiscard]] bool isComplete() const noexcept { return complete_; }

    void fillOpacity(std::span<std::uint8_t> into, std::int32_t bottomY, std::int32_t topY,
                     std::span<const std::int32_t> opacityOf) const;

    [[nodiscard]] std::int32_t stateAt(std::int32_t windowX, std::int32_t y, std::int32_t windowZ) const;

    [[nodiscard]] std::pair<std::int32_t, std::int32_t> surfaceRange() const;

    [[nodiscard]] std::optional<std::pair<std::size_t, std::size_t>>
    emitterSections(const std::function<std::int32_t(std::int32_t)>& emission) const;

private:
    [[nodiscard]] const Chunk* chunkAt(std::int32_t offsetX, std::int32_t offsetZ) const;

    std::array<const Chunk*, 9> chunks_{};
    std::int32_t minY_;
    std::int32_t maxY_;
    std::int32_t originX_;
    std::int32_t originZ_;
    bool complete_;
};

} // namespace basalt::server::world::light
