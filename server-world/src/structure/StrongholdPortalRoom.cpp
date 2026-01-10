#include <basalt/server/world/structure/StrongholdLibrary.hpp>

#include <array>
#include <cstdint>
#include <string_view>

namespace basalt::server::world::structure {
namespace {

[[nodiscard]] std::int32_t lava() {
    static const std::int32_t stateId = strongholdPalette::state("lava");
    return stateId;
}

[[nodiscard]] std::int32_t endPortal() {
    static const std::int32_t stateId = strongholdPalette::state("end_portal");
    return stateId;
}

[[nodiscard]] std::int32_t frame(std::string_view facing, bool hasEye) {
    return strongholdPalette::state("end_portal_frame",
                                    {{"facing", std::string{facing}}, {"eye", hasEye ? "true" : "false"}});
}

} // namespace

void strongholdPortalRoom::place(const StrongholdBrush& brush) {
    brush.shell(0, 0, 0, 10, 7, 15, false);
    brush.door(StrongholdDoor::Grates, 4, 1, 0);
    brush.shell(1, 6, 1, 1, 6, 14, false);
    brush.shell(9, 6, 1, 9, 6, 14, false);
    brush.shell(2, 6, 1, 8, 6, 2, false);
    brush.shell(2, 6, 14, 8, 6, 14, false);
    brush.shell(1, 1, 1, 2, 1, 4, false);
    brush.shell(8, 1, 1, 9, 1, 4, false);
    brush.box(1, 1, 1, 1, 1, 3, lava());
    brush.box(9, 1, 1, 9, 1, 3, lava());
    brush.shell(3, 1, 8, 7, 1, 12, false);
    brush.box(4, 1, 9, 6, 1, 11, lava());
    const std::int32_t northSouth = strongholdPalette::bars({"north", "south"});
    const std::int32_t westEast = strongholdPalette::bars({"west", "east"});
    for (std::int32_t z = 3; z < 14; z += 2) {
        brush.box(0, 3, z, 0, 4, z, northSouth);
        brush.box(10, 3, z, 10, 4, z, northSouth);
    }
    for (std::int32_t x = 2; x < 9; x += 2) {
        brush.box(x, 3, 15, x, 4, 15, westEast);
    }
    const std::int32_t stairs = strongholdPalette::stairs("stone_brick_stairs", "north");
    brush.shell(4, 1, 5, 6, 1, 7, false);
    brush.shell(4, 2, 6, 6, 2, 7, false);
    brush.shell(4, 3, 7, 6, 3, 7, false);
    for (std::int32_t x = 4; x <= 6; ++x) {
        brush.place(stairs, x, 1, 4);
        brush.place(stairs, x, 2, 5);
        brush.place(stairs, x, 3, 6);
    }
    // parity: the twelve eyes are drawn before any frame is placed, and all twelve are drawn even
    // when the first already fails, which is what makes a filled portal so rare.
    std::array<bool, 12> eyes{};
    bool allEyes = true;
    for (bool& eye : eyes) {
        eye = brush.random().nextFloat() > 0.9F;
        allEyes = allEyes && eye;
    }
    brush.place(frame("north", eyes[0]), 4, 3, 8);
    brush.place(frame("north", eyes[1]), 5, 3, 8);
    brush.place(frame("north", eyes[2]), 6, 3, 8);
    brush.place(frame("south", eyes[3]), 4, 3, 12);
    brush.place(frame("south", eyes[4]), 5, 3, 12);
    brush.place(frame("south", eyes[5]), 6, 3, 12);
    brush.place(frame("east", eyes[6]), 3, 3, 9);
    brush.place(frame("east", eyes[7]), 3, 3, 10);
    brush.place(frame("east", eyes[8]), 3, 3, 11);
    brush.place(frame("west", eyes[9]), 7, 3, 9);
    brush.place(frame("west", eyes[10]), 7, 3, 10);
    brush.place(frame("west", eyes[11]), 7, 3, 11);
    if (allEyes) {
        for (std::int32_t z = 9; z <= 11; ++z) {
            for (std::int32_t x = 4; x <= 6; ++x) {
                brush.place(endPortal(), x, 3, z);
            }
        }
    }
    brush.silverfishSpawner(5, 3, 6);
}

} // namespace basalt::server::world::structure
