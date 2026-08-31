#pragma once

#include <basalt/server/world/Chunk.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>

namespace basalt::server::world::light {

/// The chunk being lit and the eight around it, read straight off the chunk objects.
///
/// Three by three is not a round number, it is the reach of light: fifteen levels is less than a
/// chunk, so nothing outside this window can reach the middle one. It is also why the lookups take
/// no lock: a light pass reads a few hundred thousand blocks, and going through the world's own
/// accessor for each of them cost more than the lighting.
///
/// A neighbour that is not loaded reads as air, so light pours in from where nothing is known yet.
/// That errs bright, which is the side to err on: the black band this replaced was a chunk wide.
class LightWindow {
public:
    static constexpr std::int32_t kWidth = 48;

    using Neighbours = std::function<const Chunk*(std::int32_t chunkX, std::int32_t chunkZ)>;

    LightWindow(const Chunk& centre, const Neighbours& neighbours);

    [[nodiscard]] const Chunk& centre() const noexcept { return *chunks_[4]; }

    [[nodiscard]] std::int32_t minY() const noexcept { return minY_; }

    [[nodiscard]] std::int32_t maxY() const noexcept { return maxY_; }

    /// Whether all nine chunks were loaded: light worked out without one errs bright, so it is not
    /// kept.
    [[nodiscard]] bool isComplete() const noexcept { return complete_; }

    /// Copies the opacity of a whole band into `into`, one byte per block of the window.
    ///
    /// Section by section rather than block by block: unpacking a paletted container in one go is
    /// what the chunk codec already does to send one, and it measured an order of magnitude cheaper
    /// than asking it for four thousand blocks one at a time.
    void fillOpacity(std::span<std::uint8_t> into, std::int32_t bottomY, std::int32_t topY,
                     std::span<const std::int32_t> opacityOf) const;

    /// The block at a window coordinate, where 0 is one chunk west and north of the middle one.
    [[nodiscard]] std::int32_t stateAt(std::int32_t windowX, std::int32_t y, std::int32_t windowZ) const;

    /// The tallest and shortest surface in the window, which is the band worth lighting.
    [[nodiscard]] std::pair<std::int32_t, std::int32_t> surfaceRange() const;

    /// The sections any of the nine chunks lights up, so an unlit band costs nothing.
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
