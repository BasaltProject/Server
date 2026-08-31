#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>
#include <vector>

namespace basalt::server::world::portal {

/// Reads block states anywhere in a world, which is what a portal frame search needs.
using BlockStateView = std::function<std::int32_t(std::int32_t, std::int32_t, std::int32_t)>;

/// The two axes a Nether portal can stand on. parity: `Direction.Axis`, horizontal only.
enum class PortalAxis : std::uint8_t { X, Z };

[[nodiscard]] constexpr std::string_view serializedName(PortalAxis axis) { return axis == PortalAxis::X ? "x" : "z"; }

/// The direction the search calls "right": west for an X portal, south for a Z one.
[[nodiscard]] constexpr std::int32_t rightX(PortalAxis axis) { return axis == PortalAxis::X ? -1 : 0; }

[[nodiscard]] constexpr std::int32_t rightZ(PortalAxis axis) { return axis == PortalAxis::X ? 0 : 1; }

[[nodiscard]] constexpr PortalAxis other(PortalAxis axis) {
    return axis == PortalAxis::X ? PortalAxis::Z : PortalAxis::X;
}

/// One block of a portal's interior.
struct PortalBlock {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;

    friend bool operator==(const PortalBlock&, const PortalBlock&) = default;
};

/// A Nether portal frame, found by walking out from one block inside it.
///
/// parity: `PortalShape`, including its two odd corners. The search starts by falling to the bottom
/// of the empty column, so lighting the top of a frame finds the same portal as lighting the bottom;
/// and "empty" means air, fire or an existing portal block, which is what lets a portal be relit
/// after part of it was broken.
class PortalShape {
public:
    static constexpr std::int32_t kMinWidth = 2;
    static constexpr std::int32_t kMaxWidth = 21;
    static constexpr std::int32_t kMinHeight = 3;
    static constexpr std::int32_t kMaxHeight = 21;

    [[nodiscard]] PortalAxis axis() const noexcept { return axis_; }

    [[nodiscard]] std::int32_t bottomLeftX() const noexcept { return bottomLeftX_; }

    [[nodiscard]] std::int32_t bottomLeftY() const noexcept { return bottomLeftY_; }

    [[nodiscard]] std::int32_t bottomLeftZ() const noexcept { return bottomLeftZ_; }

    [[nodiscard]] std::int32_t width() const noexcept { return width_; }

    [[nodiscard]] std::int32_t height() const noexcept { return height_; }

    [[nodiscard]] bool isValid() const noexcept {
        return width_ >= kMinWidth && width_ <= kMaxWidth && height_ >= kMinHeight && height_ <= kMaxHeight;
    }

    /// Whether the frame is already full of portal blocks, which is what "this portal is lit" means.
    [[nodiscard]] bool isComplete() const noexcept { return isValid() && portalBlocks_ == width_ * height_; }

    [[nodiscard]] std::int32_t portalBlocks() const noexcept { return portalBlocks_; }

    /// Every block inside the frame, bottom row first.
    [[nodiscard]] std::vector<PortalBlock> interior() const;

    /// parity: `PortalShape.findAnyShape`, which answers an invalid shape rather than nothing.
    [[nodiscard]] static PortalShape findAnyShape(const BlockStateView& view, std::int32_t x, std::int32_t y,
                                                  std::int32_t z, std::int32_t minY, PortalAxis axis);

    /// parity: `PortalShape.findPortalShape`: the preferred axis first, then the other one.
    [[nodiscard]] static std::optional<PortalShape> find(const BlockStateView& view, std::int32_t x, std::int32_t y,
                                                         std::int32_t z, std::int32_t minY, PortalAxis preferred,
                                                         const std::function<bool(const PortalShape&)>& accept);

    /// parity: `PortalShape.findEmptyPortalShape`: a valid frame with nothing in it yet.
    [[nodiscard]] static std::optional<PortalShape> findEmpty(const BlockStateView& view, std::int32_t x,
                                                              std::int32_t y, std::int32_t z, std::int32_t minY,
                                                              PortalAxis preferred = PortalAxis::X);

    /// The portal block for `axis`, which is what fills a frame.
    [[nodiscard]] static std::int32_t portalState(PortalAxis axis);

    [[nodiscard]] static bool isPortal(std::int32_t state);

    /// The axis of a portal block, or empty for anything else.
    [[nodiscard]] static std::optional<PortalAxis> axisOf(std::int32_t state);

private:
    PortalShape(PortalAxis axis, std::int32_t bottomLeftX, std::int32_t bottomLeftY, std::int32_t bottomLeftZ,
                std::int32_t width, std::int32_t height, std::int32_t portalBlocks)
        : axis_(axis), bottomLeftX_(bottomLeftX), bottomLeftY_(bottomLeftY), bottomLeftZ_(bottomLeftZ), width_(width),
          height_(height), portalBlocks_(portalBlocks) {}

    PortalAxis axis_;
    std::int32_t bottomLeftX_;
    std::int32_t bottomLeftY_;
    std::int32_t bottomLeftZ_;
    std::int32_t width_;
    std::int32_t height_;
    std::int32_t portalBlocks_;
};

} // namespace basalt::server::world::portal
