#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/structure/TemplateBlockProcessors.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace basalt::server::world::structure;
namespace registry = basalt::server::registry;

namespace {

[[nodiscard]] std::int32_t stateOf(std::string_view path) { return registry::blocks::named(path)->stateId; }

/// Inputs chosen to reach every branch: the aged paths, the blackstone swaps and the drops.
[[nodiscard]] const std::vector<std::int32_t>& inputs() {
    static const std::vector<std::int32_t> built = [] {
        std::vector<std::int32_t> states;
        for (const std::string_view path :
             {"stone_bricks", "stone", "chiseled_stone_bricks", "stone_brick_stairs", "cobblestone_stairs",
              "stone_brick_slab", "cobblestone_wall", "obsidian", "structure_void", "jigsaw", "air", "netherrack",
              "cobblestone", "mossy_cobblestone_slab", "oak_slab"}) {
            states.push_back(stateOf(path));
        }
        return states;
    }();
    return built;
}

[[nodiscard]] const std::vector<std::int32_t>& worldStates() {
    static const std::vector<std::int32_t> built = [] {
        std::vector<std::int32_t> states;
        for (const std::string_view path : {"air", "stone", "lava", "chest", "bedrock", "spawner"}) {
            states.push_back(stateOf(path));
        }
        return states;
    }();
    return built;
}

struct Run {
    std::int32_t dropped = 0;
    std::int64_t checksum = 0;
};

[[nodiscard]] Run fold(const TemplateBlockProcessor& processor) {
    Run result;
    std::uint64_t checksum = 14695981039346656037ULL;
    for (std::size_t inputIndex = 0; inputIndex < inputs().size(); ++inputIndex) {
        for (std::size_t worldIndex = 0; worldIndex < worldStates().size(); ++worldIndex) {
            for (std::int32_t step = 0; step < 8; ++step) {
                const StructurePos position{static_cast<std::int32_t>(inputIndex) * 7 - 13, step * 9 - 20,
                                            static_cast<std::int32_t>(worldIndex) * 5 + step};
                const std::optional<std::int32_t> produced =
                    processor(inputs()[inputIndex], worldStates()[worldIndex], position);
                if (!produced.has_value()) {
                    ++result.dropped;
                }
                const std::int64_t value = produced.value_or(-1);
                checksum = (checksum ^ static_cast<std::uint64_t>(value)) * 1099511628211ULL;
            }
        }
    }
    result.checksum = static_cast<std::int64_t>(checksum);
    return result;
}

} // namespace

TEST_CASE("the template block processors produce what the frozen kotlin produces") {
    SUBCASE("ignoreStructureBlock") {
        const Run result = fold(templateBlockProcessors::ignoreStructureBlock());
        CHECK(result.dropped == 96);
        CHECK(result.checksum == 5023076210339678821LL);
    }

    SUBCASE("ignoreStructureAndAir") {
        const Run result = fold(templateBlockProcessors::ignoreStructureAndAir());
        CHECK(result.dropped == 144);
        CHECK(result.checksum == 8600528693131678517LL);
    }

    SUBCASE("protectFeatures") {
        const Run result = fold(templateBlockProcessors::protectFeatures());
        CHECK(result.dropped == 360);
        CHECK(result.checksum == 7311403652384246661LL);
    }

    SUBCASE("lavaSubmerged") {
        const Run result = fold(templateBlockProcessors::lavaSubmerged());
        CHECK(result.dropped == 0);
        CHECK(result.checksum == 8236651298618264029LL);
    }

    SUBCASE("blackstoneReplace") {
        const Run result = fold(templateBlockProcessors::blackstoneReplace());
        CHECK(result.dropped == 0);
        CHECK(result.checksum == 903553851891318469LL);
    }

    SUBCASE("ageNone") {
        const Run result = fold(templateBlockProcessors::age(0.0F));
        CHECK(result.dropped == 0);
        CHECK(result.checksum == -989724562792794156LL);
    }

    SUBCASE("ageHalf") {
        const Run result = fold(templateBlockProcessors::age(0.5F));
        CHECK(result.dropped == 0);
        CHECK(result.checksum == -9063926867142375741LL);
    }

    SUBCASE("ageAll") {
        const Run result = fold(templateBlockProcessors::age(1.0F));
        CHECK(result.dropped == 0);
        CHECK(result.checksum == -1475349288588102158LL);
    }

    SUBCASE("chain") {
        const Run result = fold(templateBlockProcessors::chain(
            {templateBlockProcessors::ignoreStructureAndAir(), templateBlockProcessors::protectFeatures(),
             templateBlockProcessors::blackstoneReplace(), templateBlockProcessors::age(0.5F),
             templateBlockProcessors::lavaSubmerged()}));
        CHECK(result.dropped == 432);
        CHECK(result.checksum == -1840941508033851674LL);
    }
}
