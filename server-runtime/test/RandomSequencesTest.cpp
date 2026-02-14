#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/world/RandomSequences.hpp>

#include <doctest.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

using basalt::server::runtime::world::RandomSequences;
using basalt::wire::common::Identifier;

namespace {

[[nodiscard]] std::string scratch(std::string_view name) {
    const std::filesystem::path directory = std::filesystem::path{BASALT_TEST_SCRATCH_DIR} / name;
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    std::filesystem::create_directories(directory, ignored);
    return directory.string();
}

[[nodiscard]] std::vector<std::int64_t> draw(RandomSequences& sequences, const Identifier& name, int howMany) {
    std::vector<std::int64_t> out;
    for (int index = 0; index < howMany; ++index) {
        out.push_back(sequences.of(name).nextLong());
    }
    return out;
}

} // namespace

TEST_CASE("A saved stream carries on where it left off") {
    const std::string root = scratch("random-sequences");
    const Identifier table = Identifier::vanilla("blocks/stone");

    std::vector<std::int64_t> expected;
    {
        RandomSequences sequences{1234};
        static_cast<void>(draw(sequences, table, 5));
        sequences.save(root);
        expected = draw(sequences, table, 3);
    }

    RandomSequences reopened{1234};
    reopened.load(root);
    CHECK(draw(reopened, table, 3) == expected);
}

TEST_CASE("A world with no file starts its streams from the seed") {
    const std::string root = scratch("random-sequences-empty");
    const Identifier table = Identifier::vanilla("blocks/stone");

    RandomSequences fresh{1234};
    fresh.load(root);

    RandomSequences never{1234};
    CHECK(draw(fresh, table, 3) == draw(never, table, 3));
}

TEST_CASE("Nothing is written when no stream was asked for") {
    const std::string root = scratch("random-sequences-clean");
    RandomSequences sequences{1234};
    sequences.save(root);
    CHECK_FALSE(std::filesystem::exists(std::filesystem::path{root} / "random_sequences.nbt"));
}
