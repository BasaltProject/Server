#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/world/RealTestInstances.hpp>

#include <doctest.h>

#include <map>
#include <string>
#include <vector>

using basalt::server::runtime::world::RealTestInstances;
using basalt::server::runtime::world::TestInstance;
using basalt::wire::common::Identifier;

namespace {

/// A jar that is a map of names to text, listing entries in reverse order on purpose: the loader has
/// to sort them itself.
struct FakeJar {
    std::map<std::string, std::string> files;

    [[nodiscard]] RealTestInstances::Jar handle() {
        return RealTestInstances::Jar{
            .readText = [this](std::string_view name) -> std::optional<std::string> {
                const auto found = files.find(std::string{name});
                return found == files.end() ? std::nullopt : std::optional{found->second};
            },
            .entriesUnder =
                [this](std::string_view prefix) {
                    std::vector<std::string> names;
                    for (const auto& [name, text] : files) {
                        if (name.starts_with(prefix)) {
                            names.push_back(name);
                        }
                    }
                    std::ranges::reverse(names);
                    return names;
                },
        };
    }
};

[[nodiscard]] std::string entry(const std::string& path) { return "data/minecraft/test_instance/" + path + ".json"; }

} // namespace

TEST_CASE("a function test is read whole") {
    FakeJar jar;
    jar.files[entry("example/works")] =
        R"({"type":"minecraft:function","function":"minecraft:test/works","max_ticks":250,"required":false})";
    const RealTestInstances instances{jar.handle()};

    REQUIRE(instances.all().size() == 1);
    const TestInstance& test = instances.all()[0];
    CHECK(test.id == Identifier::vanilla("example/works"));
    CHECK(test.type == Identifier::vanilla("function"));
    CHECK(test.function == Identifier::vanilla("test/works"));
    CHECK(test.maxTicks == 250);
    CHECK_FALSE(test.required);
    CHECK(test.isFunctionTest());
}

TEST_CASE("a test that names no budget gets a hundred ticks and is required") {
    FakeJar jar;
    jar.files[entry("plain")] = R"({"type":"minecraft:function","function":"minecraft:test/plain"})";
    const RealTestInstances instances{jar.handle()};

    REQUIRE(instances.all().size() == 1);
    CHECK(instances.all()[0].maxTicks == 100);
    CHECK(instances.all()[0].required);
}

TEST_CASE("a budget and a flag written as text are read, and anything else falls back") {
    FakeJar jar;
    jar.files[entry("a_text_budget")] = R"({"type":"minecraft:function","function":"f","max_ticks":"250"})";
    jar.files[entry("b_trailing_budget")] = R"({"type":"minecraft:function","function":"f","max_ticks":"25x"})";
    jar.files[entry("c_word_budget")] = R"({"type":"minecraft:function","function":"f","max_ticks":"abc"})";
    jar.files[entry("d_fractional_budget")] = R"({"type":"minecraft:function","function":"f","max_ticks":12.5})";
    jar.files[entry("e_text_flag")] = R"({"type":"minecraft:function","function":"f","required":"false"})";
    jar.files[entry("f_odd_flag")] = R"({"type":"minecraft:function","function":"f","required":"yes"})";
    jar.files[entry("g_number_flag")] = R"({"type":"minecraft:function","function":"f","required":1})";
    const RealTestInstances instances{jar.handle()};

    REQUIRE(instances.all().size() == 7);
    CHECK(instances.all()[0].maxTicks == 250);
    CHECK(instances.all()[1].maxTicks == 100);
    CHECK(instances.all()[2].maxTicks == 100);
    CHECK(instances.all()[3].maxTicks == 100);
    CHECK_FALSE(instances.all()[4].required);
    CHECK(instances.all()[5].required);
    CHECK(instances.all()[6].required);
}

TEST_CASE("a test with no function of its own is kept, and is not a function test") {
    FakeJar jar;
    jar.files[entry("a_block_based")] = R"({"type":"minecraft:block_based","structure":"minecraft:empty"})";
    jar.files[entry("b_no_function")] = R"({"type":"minecraft:function"})";
    jar.files[entry("c_other_type")] = R"({"type":"minecraft:block_based","function":"minecraft:test/other"})";
    const RealTestInstances instances{jar.handle()};

    REQUIRE(instances.all().size() == 3);
    CHECK(instances.all()[0].function == std::nullopt);
    CHECK_FALSE(instances.all()[0].isFunctionTest());
    CHECK_FALSE(instances.all()[1].isFunctionTest());
    // A function on a type that is not `function` is still not one this server can run.
    CHECK(instances.all()[2].function == Identifier::vanilla("test/other"));
    CHECK_FALSE(instances.all()[2].isFunctionTest());
}

TEST_CASE("a file that does not declare an instance is skipped and the rest still load") {
    FakeJar jar;
    jar.files[entry("a_good")] = R"({"type":"minecraft:function","function":"minecraft:test/good"})";
    jar.files[entry("b_malformed")] = "{not json";
    jar.files[entry("c_not_an_object")] = "[1,2]";
    jar.files[entry("d_no_type")] = R"({"function":"minecraft:test/no_type"})";
    jar.files[entry("e_type_is_a_number")] = R"({"type":5})";
    jar.files[entry("f_malformed_function")] = R"({"type":"minecraft:function","function":"a:b:c"})";
    jar.files[entry("g_function_is_a_number")] = R"({"type":"minecraft:function","function":5})";
    jar.files["data/minecraft/test_instance/h_not_json.mcmeta"] = "{}";
    const RealTestInstances instances{jar.handle()};

    REQUIRE(instances.all().size() == 1);
    CHECK(instances.all()[0].id == Identifier::vanilla("a_good"));
}

TEST_CASE("instances come in sorted order and answer by their own id") {
    FakeJar jar;
    jar.files[entry("zebra")] = R"({"type":"minecraft:function","function":"minecraft:test/zebra"})";
    jar.files[entry("alpha")] = R"({"type":"minecraft:function","function":"minecraft:test/alpha"})";
    jar.files[entry("middle")] = R"({"type":"minecraft:function","function":"minecraft:test/middle"})";
    const RealTestInstances instances{jar.handle()};

    REQUIRE(instances.all().size() == 3);
    CHECK(instances.all()[0].id == Identifier::vanilla("alpha"));
    CHECK(instances.all()[1].id == Identifier::vanilla("middle"));
    CHECK(instances.all()[2].id == Identifier::vanilla("zebra"));

    const TestInstance* middle = instances.find(Identifier::vanilla("middle"));
    REQUIRE(middle != nullptr);
    CHECK(middle == &instances.all()[1]);
    CHECK(instances.find(Identifier::vanilla("alpha")) == &instances.all()[0]);
    CHECK(instances.find(Identifier::vanilla("zebra")) == &instances.all()[2]);
    CHECK(instances.find(Identifier::vanilla("nothing_here")) == nullptr);
}

TEST_CASE("a jar that can list but not read declares nothing") {
    FakeJar jar;
    jar.files[entry("plain")] = R"({"type":"minecraft:function","function":"minecraft:test/plain"})";
    RealTestInstances::Jar handle = jar.handle();
    handle.readText = nullptr;
    const RealTestInstances instances{handle};

    CHECK(instances.all().empty());
}

TEST_CASE("no jar means no instances rather than a missing list") {
    const RealTestInstances instances{RealTestInstances::Jar{}};

    CHECK(instances.all().empty());
    CHECK(instances.find(Identifier::vanilla("plain")) == nullptr);
}
