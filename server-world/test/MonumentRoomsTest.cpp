#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/world/structure/MonumentRoomFitters.hpp>

#include <doctest.h>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

using namespace basalt::server::world::structure;
namespace rng = basalt::server::random;

namespace {

[[nodiscard]] std::string describe(const MonumentRoom& room) {
    std::string text = std::to_string(room.index()) + "/" + (room.claimed() ? "true" : "false") + "/" +
                       (room.isSource() ? "true" : "false") + "/" + std::to_string(room.countOpenings()) + "/";
    for (std::size_t face = 0; face < 6; ++face) {
        text += room.hasOpening(face) ? '1' : '0';
    }
    return text;
}

struct Run {
    std::int32_t rooms = 0;
    std::int32_t openings = 0;
    std::int32_t pieces = 0;
    std::int32_t draws = 0;
    std::string kinds;
    std::int64_t checksum = 0;
};

[[nodiscard]] Run run(std::int64_t seed, std::int32_t monuments) {
    Run result;
    std::uint64_t checksum = 14695981039346656037ULL;
    std::uint32_t draws = 0;
    std::array<std::int32_t, 12> kinds{};
    const auto mix = [&checksum](std::int64_t value) {
        checksum = (checksum ^ static_cast<std::uint64_t>(value)) * 1099511628211ULL;
    };
    for (std::int32_t index = 0; index < monuments; ++index) {
        rng::RandomSource random = rng::RandomSource::worldgen(rng::RandomSource::legacy(seed + index));
        MonumentRoomGraph graph = generateMonumentRoomGraph(random);
        result.rooms += static_cast<std::int32_t>(graph.rooms.size());
        for (const MonumentRoom* room : graph.rooms) {
            result.openings += room->countOpenings();
            for (const char letter : describe(*room)) {
                mix(static_cast<std::int64_t>(letter));
            }
        }
        mix(graph.coreRoom->index());
        const std::vector<MonumentRoomPiece> fitted = fitMonumentRooms(graph, random);
        result.pieces += static_cast<std::int32_t>(fitted.size());
        for (const MonumentRoomPiece& piece : fitted) {
            ++kinds[static_cast<std::size_t>(piece.kind)];
            mix(piece.room == nullptr ? -1 : piece.room->index());
            mix(piece.design);
            mix(static_cast<std::int64_t>(piece.kind));
        }
        draws = draws * 31U + static_cast<std::uint32_t>(random.nextInt(1 << 20));
    }
    for (const std::int32_t count : kinds) {
        if (!result.kinds.empty()) {
            result.kinds += ',';
        }
        result.kinds += std::to_string(count);
    }
    result.draws = static_cast<std::int32_t>(draws);
    result.checksum = static_cast<std::int64_t>(checksum);
    return result;
}

void check(const Run& result, std::int32_t rooms, std::int32_t openings, std::int32_t pieces, std::int32_t draws,
           std::string_view kinds, std::int64_t checksum) {
    CHECK(result.rooms == rooms);
    CHECK(result.openings == openings);
    CHECK(result.pieces == pieces);
    CHECK(result.draws == draws);
    CHECK(result.kinds == kinds);
    CHECK(result.checksum == checksum);
}

} // namespace

TEST_CASE("the monument lattice and its rooms come out as the frozen kotlin's") {
    SUBCASE("first") {
        check(run(20250831, 24), 1176, 2287, 759, -1774604336, "24,24,81,1,62,3,94,368,30,24,24,24",
              -7341362444674436618LL);
    }

    SUBCASE("second") {
        check(run(1, 24), 1176, 2288, 757, 1795868331, "24,24,77,2,70,5,83,362,38,24,24,24", -3684140691443722969LL);
    }

    SUBCASE("third") {
        check(run(-777, 24), 1176, 2285, 764, 2038886183, "24,24,81,2,71,0,86,376,28,24,24,24", 2427154034732940493LL);
    }
}
