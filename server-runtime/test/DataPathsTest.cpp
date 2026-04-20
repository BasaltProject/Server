#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/command/DataPaths.hpp>
#include <basalt/wire/nbt/Snbt.hpp>

#include <doctest.h>

#include <string>

namespace dataPaths = basalt::server::runtime::command::dataPaths;

using basalt::wire::nbt::NbtCompound;
using basalt::wire::nbt::NbtTag;

namespace {

[[nodiscard]] NbtCompound compoundOf(const std::string& text) {
    const auto parsed = basalt::wire::nbt::snbt::parse(text);
    REQUIRE(parsed.has_value());
    const auto* compound = parsed->get<NbtCompound>();
    REQUIRE(compound != nullptr);
    return *compound;
}

[[nodiscard]] std::string printed(const NbtTag& tag) { return basalt::wire::nbt::snbt::stringify(tag); }

} // namespace

TEST_CASE("a dotted path walks nested compounds") {
    const NbtCompound root = compoundOf(R"({a:{b:{c:7}}})");
    const std::optional<NbtTag> found = dataPaths::resolve(NbtTag::ofCompound(root), "a.b.c");
    REQUIRE(found.has_value());
    CHECK(printed(*found) == "7");
    CHECK_FALSE(dataPaths::resolve(NbtTag::ofCompound(root), "a.b.d").has_value());
    CHECK_FALSE(dataPaths::resolve(NbtTag::ofCompound(root), "a.x.c").has_value());
}

TEST_CASE("a list is read by index and by filter") {
    const NbtCompound root = compoundOf(R"({crew:[{name:"bo",age:9},{name:"jo",age:4}]})");
    const std::optional<NbtTag> byIndex = dataPaths::resolve(NbtTag::ofCompound(root), "crew[1].name");
    REQUIRE(byIndex.has_value());
    CHECK(printed(*byIndex) == "\"jo\"");

    const std::optional<NbtTag> byFilter = dataPaths::resolve(NbtTag::ofCompound(root), R"(crew[{name:"bo"}].age)");
    REQUIRE(byFilter.has_value());
    CHECK(printed(*byFilter) == "9");

    const NbtCompound dotted = compoundOf(R"({crew:[{id:"a.b",age:1}]})");
    CHECK(dataPaths::resolve(NbtTag::ofCompound(dotted), R"(crew[{id:"a.b"}].age)").has_value());

    CHECK_FALSE(dataPaths::resolve(NbtTag::ofCompound(root), "crew[9].name").has_value());
    CHECK_FALSE(dataPaths::resolve(NbtTag::ofCompound(root), R"(crew[{name:"zz"}])").has_value());
}

TEST_CASE("a filter matches only when every key it names matches") {
    const NbtCompound root = compoundOf(R"({crew:[{name:"bo",rank:{level:2}},{name:"jo",rank:{level:5}}]})");
    CHECK(dataPaths::resolve(NbtTag::ofCompound(root), R"(crew[{rank:{level:5}}].name)").has_value());
    CHECK_FALSE(dataPaths::resolve(NbtTag::ofCompound(root), R"(crew[{name:"bo",rank:{level:5}}])").has_value());
}

TEST_CASE("merge takes the incoming value except where both sides are compounds") {
    const NbtCompound base = compoundOf(R"({a:{x:1,y:2},b:3})");
    const NbtCompound incoming = compoundOf(R"({a:{y:9,z:8},b:{deep:1}})");
    const NbtCompound merged = dataPaths::merge(base, incoming);

    CHECK(printed(*dataPaths::resolve(NbtTag::ofCompound(merged), "a.x")) == "1");
    CHECK(printed(*dataPaths::resolve(NbtTag::ofCompound(merged), "a.y")) == "9");
    CHECK(printed(*dataPaths::resolve(NbtTag::ofCompound(merged), "a.z")) == "8");
    CHECK(dataPaths::resolve(NbtTag::ofCompound(merged), "b.deep").has_value());
}

TEST_CASE("set writes a chain of keys and leaves the original alone") {
    const NbtCompound root = compoundOf(R"({a:{b:1}})");
    const std::optional<NbtCompound> written = dataPaths::set(root, "a.c.d", NbtTag::ofInt(5));
    REQUIRE(written.has_value());
    CHECK(printed(*dataPaths::resolve(NbtTag::ofCompound(*written), "a.c.d")) == "5");
    CHECK(printed(*dataPaths::resolve(NbtTag::ofCompound(*written), "a.b")) == "1");
    CHECK_FALSE(dataPaths::resolve(NbtTag::ofCompound(root), "a.c.d").has_value());

    CHECK_FALSE(dataPaths::set(root, "a[0]", NbtTag::ofInt(1)).has_value());
    CHECK_FALSE(dataPaths::set(root, "", NbtTag::ofInt(1)).has_value());
}

TEST_CASE("set and remove reach list elements by index, from the end, and by filter") {
    const NbtCompound root = compoundOf(R"({Pos:[1.0d,2.0d,3.0d],Items:[{id:"a",n:1},{id:"b",n:2}]})");
    const std::optional<NbtCompound> moved = dataPaths::set(root, "Pos[1]", NbtTag::ofDouble(70.0));
    REQUIRE(moved.has_value());
    CHECK(*dataPaths::resolve(NbtTag::ofCompound(*moved), "Pos[1]") == NbtTag::ofDouble(70.0));
    CHECK(*dataPaths::resolve(NbtTag::ofCompound(*moved), "Pos[-1]") == NbtTag::ofDouble(3.0));
    CHECK_FALSE(dataPaths::set(root, "Pos[0]", NbtTag::ofInt(1)).has_value());

    const std::optional<NbtCompound> counted = dataPaths::set(root, R"(Items[{id:"b"}].n)", NbtTag::ofInt(9));
    REQUIRE(counted.has_value());
    CHECK(printed(*dataPaths::resolve(NbtTag::ofCompound(*counted), "Items[1].n")) == "9");

    const std::optional<NbtCompound> dropped = dataPaths::remove(root, "Items[0]");
    REQUIRE(dropped.has_value());
    CHECK(printed(*dataPaths::resolve(NbtTag::ofCompound(*dropped), "Items[0].id")) == R"("b")");
    CHECK_FALSE(dataPaths::remove(root, "Items[5]").has_value());
}

TEST_CASE("remove drops what the path names and refuses what was never there") {
    const NbtCompound root = compoundOf(R"({a:{b:1,c:2}})");
    const std::optional<NbtCompound> without = dataPaths::remove(root, "a.b");
    REQUIRE(without.has_value());
    CHECK_FALSE(dataPaths::resolve(NbtTag::ofCompound(*without), "a.b").has_value());
    CHECK(dataPaths::resolve(NbtTag::ofCompound(*without), "a.c").has_value());

    CHECK_FALSE(dataPaths::remove(root, "a.z").has_value());
    CHECK_FALSE(dataPaths::remove(root, "z.b").has_value());
}

TEST_CASE("a malformed path is refused rather than half read") {
    const NbtCompound root = compoundOf(R"({a:[1,2]})");
    CHECK_FALSE(dataPaths::steps("a[").has_value());
    CHECK_FALSE(dataPaths::steps("a[nine]").has_value());
    CHECK_FALSE(dataPaths::resolve(NbtTag::ofCompound(root), "a[").has_value());
}
