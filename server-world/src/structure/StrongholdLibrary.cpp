#include <basalt/server/world/structure/StrongholdLibrary.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <initializer_list>
#include <string_view>

namespace basalt::server::world::structure {
namespace {

constexpr std::string_view kLibraryLoot = "minecraft:chests/stronghold_library";

[[nodiscard]] std::int32_t planks() {
    static const std::int32_t stateId = strongholdPalette::state("oak_planks");
    return stateId;
}

[[nodiscard]] std::int32_t bookshelf() {
    static const std::int32_t stateId = strongholdPalette::state("bookshelf");
    return stateId;
}

[[nodiscard]] std::int32_t cobweb() {
    static const std::int32_t stateId = strongholdPalette::state("cobweb");
    return stateId;
}

[[nodiscard]] std::int32_t torch() {
    static const std::int32_t stateId = strongholdPalette::state("torch");
    return stateId;
}

[[nodiscard]] std::int32_t ladderSouth() {
    static const std::int32_t stateId =
        strongholdPalette::state("ladder", {{"facing", "south"}, {"waterlogged", "false"}});
    return stateId;
}

[[nodiscard]] std::int32_t fence(std::initializer_list<std::string_view> sides) {
    const auto on = [&sides](std::string_view side) {
        return std::find(sides.begin(), sides.end(), side) != sides.end() ? "true" : "false";
    };
    return strongholdPalette::state("oak_fence", {{"north", on("north")},
                                                  {"south", on("south")},
                                                  {"east", on("east")},
                                                  {"west", on("west")},
                                                  {"waterlogged", "false"}});
}

void balcony(const StrongholdBrush& brush) {
    brush.box(1, 5, 1, 3, 5, 13, planks());
    brush.box(10, 5, 1, 12, 5, 13, planks());
    brush.box(4, 5, 1, 9, 5, 2, planks());
    brush.box(4, 5, 12, 9, 5, 13, planks());
    brush.place(planks(), 9, 5, 11);
    brush.place(planks(), 8, 5, 11);
    brush.place(planks(), 9, 5, 10);
    const std::int32_t weFence = fence({"west", "east"});
    const std::int32_t nsFence = fence({"north", "south"});
    brush.box(3, 6, 3, 3, 6, 11, nsFence);
    brush.box(10, 6, 3, 10, 6, 9, nsFence);
    brush.box(4, 6, 2, 9, 6, 2, weFence);
    brush.box(4, 6, 12, 7, 6, 12, weFence);
    brush.place(fence({"north", "east"}), 3, 6, 2);
    brush.place(fence({"south", "east"}), 3, 6, 12);
    brush.place(fence({"north", "west"}), 10, 6, 2);
    for (std::int32_t step = 0; step <= 2; ++step) {
        brush.place(fence({"south", "west"}), 8 + step, 6, 12 - step);
        if (step != 2) {
            brush.place(fence({"north", "east"}), 8 + step, 6, 11 - step);
        }
    }
    for (std::int32_t y = 1; y <= 7; ++y) {
        brush.place(ladderSouth(), 10, y, 13);
    }
    const std::int32_t eFence = fence({"east"});
    const std::int32_t wFence = fence({"west"});
    brush.place(eFence, 6, 9, 7);
    brush.place(wFence, 7, 9, 7);
    brush.place(eFence, 6, 8, 7);
    brush.place(wFence, 7, 8, 7);
    const std::int32_t cross = fence({"north", "south", "west", "east"});
    brush.place(cross, 6, 7, 7);
    brush.place(cross, 7, 7, 7);
    brush.place(eFence, 5, 7, 7);
    brush.place(wFence, 8, 7, 7);
    brush.place(fence({"east", "north"}), 6, 7, 6);
    brush.place(fence({"east", "south"}), 6, 7, 8);
    brush.place(fence({"west", "north"}), 7, 7, 6);
    brush.place(fence({"west", "south"}), 7, 7, 8);
    brush.place(torch(), 5, 8, 7);
    brush.place(torch(), 8, 8, 7);
    brush.place(torch(), 6, 8, 6);
    brush.place(torch(), 6, 8, 8);
    brush.place(torch(), 7, 8, 6);
    brush.place(torch(), 7, 8, 8);
}

} // namespace

void strongholdLibrary::place(const StrongholdBrush& brush, const StrongholdPiece& piece) {
    const std::int32_t height = piece.tall ? 11 : 6;
    brush.shell(0, 0, 0, 13, height - 1, 14);
    brush.door(piece.entryDoor, 4, 1, 0);
    // parity: the cobwebs draw over the whole 10 by 4 by 13 volume, one roll per block, before any shelf goes in.
    brush.fill().generateMaybeBox(brush.random(), 0.07F, 2, 1, 1, 11, 4, 13, cobweb(), cobweb(), false, false);
    for (std::int32_t depth = 1; depth <= 13; ++depth) {
        if ((depth - 1) % 4 == 0) {
            brush.box(1, 1, depth, 1, 4, depth, planks());
            brush.box(12, 1, depth, 12, 4, depth, planks());
            brush.place(strongholdPalette::wallTorch("east"), 2, 3, depth);
            brush.place(strongholdPalette::wallTorch("west"), 11, 3, depth);
            if (piece.tall) {
                brush.box(1, 6, depth, 1, 9, depth, planks());
                brush.box(12, 6, depth, 12, 9, depth, planks());
            }
        } else {
            brush.box(1, 1, depth, 1, 4, depth, bookshelf());
            brush.box(12, 1, depth, 12, 4, depth, bookshelf());
            if (piece.tall) {
                brush.box(1, 6, depth, 1, 9, depth, bookshelf());
                brush.box(12, 6, depth, 12, 9, depth, bookshelf());
            }
        }
    }
    for (std::int32_t depth = 3; depth < 12; depth += 2) {
        brush.box(3, 1, depth, 4, 3, depth, bookshelf());
        brush.box(6, 1, depth, 7, 3, depth, bookshelf());
        brush.box(9, 1, depth, 10, 3, depth, bookshelf());
    }
    if (piece.tall) {
        balcony(brush);
    }
    brush.createChest(3, 3, 5, kLibraryLoot);
    if (piece.tall) {
        brush.place(strongholdPalette::caveAir(), 12, 9, 1);
        brush.createChest(12, 8, 1, kLibraryLoot);
    }
}

} // namespace basalt::server::world::structure
