#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/structure/StructureTransform.hpp>

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace basalt::server::world::structure {

/// parity: `OceanMonumentPieces.RoomDefinition`.
class MonumentRoom {
public:
    explicit MonumentRoom(std::int32_t index) : index_(index) {}

    [[nodiscard]] std::int32_t index() const noexcept { return index_; }

    [[nodiscard]] bool claimed() const noexcept { return claimed_; }

    void claim() noexcept { claimed_ = true; }

    [[nodiscard]] bool isSource() const noexcept { return isSource_; }

    void markSource() noexcept { isSource_ = true; }

    [[nodiscard]] bool isSpecial() const noexcept { return index_ >= 75; }

    [[nodiscard]] MonumentRoom* connection(std::size_t face) const noexcept { return connections_[face]; }

    [[nodiscard]] bool hasOpening(std::size_t face) const noexcept { return hasOpening_[face]; }

    void setOpening(std::size_t face, bool open) noexcept { hasOpening_[face] = open; }

    void connect(StructureDirection direction, MonumentRoom& other);

    void updateOpenings();

    [[nodiscard]] bool findSource(std::int32_t scan);

    [[nodiscard]] std::int32_t countOpenings() const;

private:
    std::int32_t index_;
    std::array<MonumentRoom*, 6> connections_{};
    std::array<bool, 6> hasOpening_{};
    bool claimed_ = false;
    bool isSource_ = false;
    std::int32_t scanIndex_ = 0;
};

struct MonumentRoomGraph {
    std::vector<std::unique_ptr<MonumentRoom>> owned;
    std::vector<MonumentRoom*> rooms;
    MonumentRoom* sourceRoom = nullptr;
    MonumentRoom* coreRoom = nullptr;
};

/// parity: `MonumentBuilding.generateRoomGraph`.
[[nodiscard]] MonumentRoomGraph generateMonumentRoomGraph(random::RandomSource& random);

/// parity: `OceanMonumentPiece.getRoomIndex`.
[[nodiscard]] constexpr std::int32_t roomIndex(std::int32_t x, std::int32_t y, std::int32_t z) noexcept {
    return y * 25 + z * 5 + x;
}

inline constexpr std::int32_t kMonumentSourceIndex = roomIndex(2, 0, 0);
inline constexpr std::int32_t kMonumentTopConnectIndex = roomIndex(2, 2, 0);
inline constexpr std::int32_t kMonumentLeftWingConnectIndex = roomIndex(0, 1, 0);
inline constexpr std::int32_t kMonumentRightWingConnectIndex = roomIndex(4, 1, 0);
inline constexpr std::int32_t kMonumentLeftWingIndex = 1001;
inline constexpr std::int32_t kMonumentRightWingIndex = 1002;
inline constexpr std::int32_t kMonumentPenthouseIndex = 1003;

} // namespace basalt::server::world::structure
