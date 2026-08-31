#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/portal/PortalShape.hpp>

#include <doctest.h>

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>

using namespace basalt::server::world::portal;
namespace registry = basalt::server::registry;

namespace {

[[nodiscard]] std::int32_t stateIdOf(std::string_view path) { return registry::blocks::named(path)->stateId; }

/// A world holding one frame on `axis` with the given interior size, optionally lit and optionally
/// with one frame block knocked out.
[[nodiscard]] BlockStateView frame(PortalAxis axis, std::int32_t width, std::int32_t height, bool lit = false,
                                   bool broken = false, std::int32_t litRows = -1, bool burning = false) {
    if (litRows < 0) {
        litRows = height;
    }
    const std::int32_t obsidian = stateIdOf("obsidian");
    const std::int32_t portal = PortalShape::portalState(axis);
    auto blocks = std::make_shared<std::map<std::tuple<std::int32_t, std::int32_t, std::int32_t>, std::int32_t>>();
    const std::int32_t rx = rightX(axis);
    const std::int32_t rz = rightZ(axis);
    for (std::int32_t column = -1; column <= width; ++column) {
        for (std::int32_t row = -1; row <= height; ++row) {
            const std::int32_t x = 100 + rx * column;
            const std::int32_t z = 200 + rz * column;
            const std::int32_t y = 64 + row;
            const bool edge = column == -1 || column == width || row == -1 || row == height;
            std::int32_t state = registry::blocks::air().stateId;
            if (edge) {
                state = broken && column == 0 && row == height ? registry::blocks::air().stateId : obsidian;
            } else if (burning && column == 0 && row == 0) {
                state = stateIdOf("fire");
            } else if (lit && row < litRows) {
                state = portal;
            }
            blocks->emplace(std::tuple{x, y, z}, state);
        }
    }
    return [blocks](std::int32_t x, std::int32_t y, std::int32_t z) {
        const auto found = blocks->find(std::tuple{x, y, z});
        return found == blocks->end() ? registry::blocks::stone().stateId : found->second;
    };
}

[[nodiscard]] std::string describe(const PortalShape& shape) {
    return std::string{"axis="} + (shape.axis() == PortalAxis::X ? "X" : "Z") +
           " x=" + std::to_string(shape.bottomLeftX()) + " y=" + std::to_string(shape.bottomLeftY()) +
           " z=" + std::to_string(shape.bottomLeftZ()) + " w=" + std::to_string(shape.width()) +
           " h=" + std::to_string(shape.height()) + " valid=" + (shape.isValid() ? "true" : "false") +
           " complete=" + (shape.isComplete() ? "true" : "false") +
           " interior=" + std::to_string(shape.interior().size());
}

} // namespace

TEST_CASE("the portal frame search finds what the frozen kotlin finds") {
    SUBCASE("any") {
        for (const PortalAxis axis : {PortalAxis::X, PortalAxis::Z}) {
            const std::optional<PortalShape> shape =
                std::optional<PortalShape>{PortalShape::findAnyShape(frame(axis, 2, 3), 100, 64, 200, -64, axis)};
            const std::string expected = axis == PortalAxis::X
                                             ? "axis=X x=100 y=64 z=200 w=2 h=3 valid=true complete=false interior=6"
                                             : "axis=Z x=100 y=64 z=200 w=2 h=3 valid=true complete=false interior=6";
            REQUIRE(shape.has_value());
            CHECK(describe(*shape) == expected);
        }
    }

    SUBCASE("empty") {
        for (const PortalAxis axis : {PortalAxis::X, PortalAxis::Z}) {
            const std::optional<PortalShape> shape = PortalShape::findEmpty(frame(axis, 2, 3), 100, 64, 200, -64, axis);
            const std::string expected = axis == PortalAxis::X
                                             ? "axis=X x=100 y=64 z=200 w=2 h=3 valid=true complete=false interior=6"
                                             : "axis=Z x=100 y=64 z=200 w=2 h=3 valid=true complete=false interior=6";
            REQUIRE(shape.has_value());
            CHECK(describe(*shape) == expected);
        }
    }

    SUBCASE("fromTop") {
        for (const PortalAxis axis : {PortalAxis::X, PortalAxis::Z}) {
            const std::optional<PortalShape> shape = PortalShape::findEmpty(frame(axis, 3, 4), 100, 66, 200, -64, axis);
            const std::string expected = axis == PortalAxis::X
                                             ? "axis=X x=100 y=64 z=200 w=3 h=4 valid=true complete=false interior=12"
                                             : "axis=Z x=100 y=64 z=200 w=3 h=4 valid=true complete=false interior=12";
            REQUIRE(shape.has_value());
            CHECK(describe(*shape) == expected);
        }
    }

    SUBCASE("lit") {
        for (const PortalAxis axis : {PortalAxis::X, PortalAxis::Z}) {
            const std::optional<PortalShape> shape =
                std::optional<PortalShape>{PortalShape::findAnyShape(frame(axis, 2, 3, true), 100, 64, 200, -64, axis)};
            const std::string expected = axis == PortalAxis::X
                                             ? "axis=X x=100 y=64 z=200 w=2 h=3 valid=true complete=true interior=6"
                                             : "axis=Z x=100 y=64 z=200 w=2 h=3 valid=true complete=true interior=6";
            REQUIRE(shape.has_value());
            CHECK(describe(*shape) == expected);
        }
    }

    SUBCASE("partlyLit") {
        for (const PortalAxis axis : {PortalAxis::X, PortalAxis::Z}) {
            const std::optional<PortalShape> shape = std::optional<PortalShape>{
                PortalShape::findAnyShape(frame(axis, 2, 3, true, false, 1), 100, 64, 200, -64, axis)};
            const std::string expected = axis == PortalAxis::X
                                             ? "axis=X x=100 y=64 z=200 w=2 h=3 valid=true complete=false interior=6"
                                             : "axis=Z x=100 y=64 z=200 w=2 h=3 valid=true complete=false interior=6";
            REQUIRE(shape.has_value());
            CHECK(describe(*shape) == expected);
        }
    }

    SUBCASE("broken") {
        for (const PortalAxis axis : {PortalAxis::X, PortalAxis::Z}) {
            const std::optional<PortalShape> shape =
                PortalShape::findEmpty(frame(axis, 2, 3, false, true), 100, 64, 200, -64, axis);
            const std::string expected = axis == PortalAxis::X ? "" : "";
            CHECK_FALSE(shape.has_value());
            static_cast<void>(expected);
        }
    }

    SUBCASE("burning") {
        // Fire inside the frame still counts as empty, which is what relighting relies on.
        for (const PortalAxis axis : {PortalAxis::X, PortalAxis::Z}) {
            const std::optional<PortalShape> shape =
                PortalShape::findEmpty(frame(axis, 2, 3, false, false, -1, true), 100, 64, 200, -64, axis);
            const std::string expected = axis == PortalAxis::X
                                             ? "axis=X x=100 y=64 z=200 w=2 h=3 valid=true complete=false interior=6"
                                             : "axis=Z x=100 y=64 z=200 w=2 h=3 valid=true complete=false interior=6";
            REQUIRE(shape.has_value());
            CHECK(describe(*shape) == expected);
        }
    }

    SUBCASE("tooNarrow") {
        for (const PortalAxis axis : {PortalAxis::X, PortalAxis::Z}) {
            CHECK_FALSE(PortalShape::findEmpty(frame(axis, 1, 4), 100, 64, 200, -64, axis).has_value());
        }
    }

    SUBCASE("wide") {
        for (const PortalAxis axis : {PortalAxis::X, PortalAxis::Z}) {
            const std::optional<PortalShape> shape =
                PortalShape::findEmpty(frame(axis, 21, 21), 100, 64, 200, -64, axis);
            const std::string expected =
                axis == PortalAxis::X ? "axis=X x=100 y=64 z=200 w=21 h=21 valid=true complete=false interior=441"
                                      : "axis=Z x=100 y=64 z=200 w=21 h=21 valid=true complete=false interior=441";
            REQUIRE(shape.has_value());
            CHECK(describe(*shape) == expected);
        }
    }

    SUBCASE("tooWide") {
        for (const PortalAxis axis : {PortalAxis::X, PortalAxis::Z}) {
            const std::optional<PortalShape> shape =
                PortalShape::findEmpty(frame(axis, 22, 5), 100, 64, 200, -64, axis);
            const std::string expected = axis == PortalAxis::X ? "" : "";
            CHECK_FALSE(shape.has_value());
            static_cast<void>(expected);
        }
    }

    SUBCASE("tooShort") {
        for (const PortalAxis axis : {PortalAxis::X, PortalAxis::Z}) {
            const std::optional<PortalShape> shape = PortalShape::findEmpty(frame(axis, 3, 2), 100, 64, 200, -64, axis);
            const std::string expected = axis == PortalAxis::X ? "" : "";
            CHECK_FALSE(shape.has_value());
            static_cast<void>(expected);
        }
    }

    SUBCASE("otherAxis") {
        for (const PortalAxis axis : {PortalAxis::X, PortalAxis::Z}) {
            const std::optional<PortalShape> shape =
                PortalShape::findEmpty(frame(axis, 2, 3), 100, 64, 200, -64, other(axis));
            const std::string expected = axis == PortalAxis::X
                                             ? "axis=X x=100 y=64 z=200 w=2 h=3 valid=true complete=false interior=6"
                                             : "axis=Z x=100 y=64 z=200 w=2 h=3 valid=true complete=false interior=6";
            REQUIRE(shape.has_value());
            CHECK(describe(*shape) == expected);
        }
    }
}

TEST_CASE("the portal block states match the frozen kotlin") {
    CHECK(PortalShape::portalState(PortalAxis::X) == 7017);
    CHECK(PortalShape::portalState(PortalAxis::Z) == 7018);
    CHECK(PortalShape::isPortal(PortalShape::portalState(PortalAxis::X)));
    CHECK(PortalShape::axisOf(PortalShape::portalState(PortalAxis::X)).value() == PortalAxis::X);
    CHECK(PortalShape::axisOf(PortalShape::portalState(PortalAxis::Z)).value() == PortalAxis::Z);
    CHECK_FALSE(PortalShape::axisOf(registry::blocks::stone().stateId).has_value());
    CHECK_FALSE(PortalShape::isPortal(registry::blocks::stone().stateId));
}
