#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/structure/StructureTransform.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace basalt::server::world::structure {

/// parity: `WoodlandMansionPieces.SimpleGrid`. Reads outside its bounds answer `valueIfOutside`
/// instead of failing, which the mansion layout leans on constantly.
class SimpleGrid {
public:
    SimpleGrid(std::int32_t width, std::int32_t height, std::int32_t valueIfOutside)
        : width_(width), height_(height), valueIfOutside_(valueIfOutside),
          cells_(static_cast<std::size_t>(width * height), 0) {}

    [[nodiscard]] std::int32_t width() const noexcept { return width_; }

    [[nodiscard]] std::int32_t height() const noexcept { return height_; }

    [[nodiscard]] std::int32_t at(std::int32_t x, std::int32_t y) const;

    void set(std::int32_t x, std::int32_t y, std::int32_t value);
    void set(std::int32_t x0, std::int32_t y0, std::int32_t x1, std::int32_t y1, std::int32_t value);
    void setIf(std::int32_t x, std::int32_t y, std::int32_t ifValue, std::int32_t value);

    [[nodiscard]] bool edgesTo(std::int32_t x, std::int32_t y, std::int32_t ifValue) const;

private:
    std::int32_t width_;
    std::int32_t height_;
    std::int32_t valueIfOutside_;
    std::vector<std::int32_t> cells_;
};

/// parity: `WoodlandMansionPieces.MansionGrid`, the floor plan on an eleven by eleven board, packed
/// into one int per cell with the room's size, id and flags. The second coordinate is called `y`
/// here as in the jar, but it is the Z axis.
class MansionGrid {
public:
    static constexpr std::int32_t kSize = 11;
    static constexpr std::int32_t kEntranceX = 7;
    static constexpr std::int32_t kEntranceY = 4;

    static constexpr std::int32_t kClear = 0;
    static constexpr std::int32_t kCorridor = 1;
    static constexpr std::int32_t kRoom = 2;
    static constexpr std::int32_t kStartRoom = 3;
    static constexpr std::int32_t kBlocked = 5;

    static constexpr std::int32_t kRoom1x1 = 65536;
    static constexpr std::int32_t kRoom1x2 = 131072;
    static constexpr std::int32_t kRoom2x2 = 262144;
    static constexpr std::int32_t kRoomOriginFlag = 1048576;
    static constexpr std::int32_t kRoomDoorFlag = 2097152;
    static constexpr std::int32_t kRoomStairsFlag = 4194304;
    static constexpr std::int32_t kRoomCorridorFlag = 8388608;
    static constexpr std::int32_t kRoomTypeMask = 983040;
    static constexpr std::int32_t kRoomIdMask = 65535;

    explicit MansionGrid(random::RandomSource& random);

    [[nodiscard]] const SimpleGrid& baseGrid() const noexcept { return baseGrid_; }

    [[nodiscard]] const SimpleGrid& thirdFloorGrid() const noexcept { return thirdFloorGrid_; }

    [[nodiscard]] const SimpleGrid& floorRooms(std::size_t floor) const { return floorRooms_[floor]; }

    [[nodiscard]] bool isRoomId(std::int32_t x, std::int32_t y, std::size_t floor, std::int32_t roomId) const;

    /// The neighbour a 1x2 room extends into, or nothing when the cell is not part of one.
    [[nodiscard]] std::optional<StructureDirection> roomDirection1x2(std::int32_t x, std::int32_t y, std::size_t floor,
                                                                     std::int32_t roomId) const;

    /// parity: `Direction.Plane.HORIZONTAL`, north, east, south, west in that order.
    [[nodiscard]] static const std::array<StructureDirection, 4>& horizontal();
    /// parity: `Direction.from2DDataValue`, which is south, west, north, east.
    [[nodiscard]] static const std::array<StructureDirection, 4>& by2d();

    [[nodiscard]] static bool isHouse(const SimpleGrid& grid, std::int32_t x, std::int32_t y);
    [[nodiscard]] static StructureDirection clockWise(StructureDirection direction);
    [[nodiscard]] static StructureDirection counterClockWise(StructureDirection direction);

private:
    void recursiveCorridor(SimpleGrid& grid, std::int32_t x, std::int32_t y, StructureDirection heading,
                           std::int32_t depth);
    /// Fills in the dents an L of corridors leaves, until nothing changes.
    [[nodiscard]] static bool cleanEdges(SimpleGrid& grid);
    /// The tower: one 1x2 room of the second floor grows a staircase and a short corridor.
    void setupThirdFloor();
    /// Groups the free cells into rooms of 1x1, 1x2 or 2x2 and gives each an id and a door. The
    /// shuffle is what makes two mansions differ, so it has to be vanilla's own.
    void identifyRooms(const SimpleGrid& fromGrid, SimpleGrid& roomGrid);
    /// parity: `Util.shuffle`, which walks from the end and swaps into the tail.
    void shuffle(std::vector<std::pair<std::int32_t, std::int32_t>>& list);

    random::RandomSource* random_;
    SimpleGrid baseGrid_{kSize, kSize, kBlocked};
    SimpleGrid thirdFloorGrid_{kSize, kSize, kBlocked};
    std::array<SimpleGrid, 3> floorRooms_{SimpleGrid{kSize, kSize, kBlocked}, SimpleGrid{kSize, kSize, kBlocked},
                                          SimpleGrid{kSize, kSize, kBlocked}};
};

} // namespace basalt::server::world::structure
