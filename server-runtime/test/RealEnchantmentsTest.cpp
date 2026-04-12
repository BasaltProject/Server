#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/world/RealEnchantments.hpp>
#include <basalt/wire/common/logging/LogOutput.hpp>

#include <doctest.h>

#include <algorithm>
#include <map>
#include <string>
#include <vector>

using basalt::server::runtime::world::RealEnchantments;
using basalt::wire::common::Identifier;
using basalt::wire::nbt::NbtCompound;

namespace {

/// A jar that is a map of names to text. It lists entries in reverse order on purpose: the registry
/// has to sort them itself, because the index of an entry is the id the wire carries.
struct FakeJar {
    std::map<std::string, std::string> files;

    [[nodiscard]] RealEnchantments::Jar handle() {
        return RealEnchantments::Jar{
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

[[nodiscard]] std::string entry(const std::string& path) { return "data/minecraft/enchantment/" + path + ".json"; }

/// Every log line this registry writes, so the reason an operator is given can be asserted.
struct CapturedLog {
    std::vector<std::string> lines;

    CapturedLog() {
        basalt::wire::common::logOutput::addSink([this](std::string_view line) { lines.emplace_back(line); });
    }

    CapturedLog(const CapturedLog&) = delete;
    CapturedLog& operator=(const CapturedLog&) = delete;

    ~CapturedLog() { basalt::wire::common::logOutput::clearSinks(); }
};

} // namespace

TEST_CASE("every enchantment the jar ships is an entry, sorted, and its index is its protocol id") {
    FakeJar jar;
    jar.files[entry("sharpness")] = R"({"max_level":5})";
    jar.files[entry("efficiency")] = R"({"max_level":5})";
    jar.files[entry("unbreaking")] = R"({"max_level":3})";
    jar.files["data/minecraft/enchantment/sharpness.mcmeta"] = "{}";
    const RealEnchantments enchantments{jar.handle()};

    REQUIRE(enchantments.size() == 3);
    CHECK(enchantments.entries()[0].first == Identifier::vanilla("efficiency"));
    CHECK(enchantments.entries()[1].first == Identifier::vanilla("sharpness"));
    CHECK(enchantments.entries()[2].first == Identifier::vanilla("unbreaking"));
    CHECK(enchantments.protocolIdOf(Identifier::vanilla("efficiency")) == 0);
    CHECK(enchantments.protocolIdOf(Identifier::vanilla("sharpness")) == 1);
    CHECK(enchantments.protocolIdOf(Identifier::vanilla("unbreaking")) == 2);
    CHECK(enchantments.ids() == std::vector<Identifier>{Identifier::vanilla("efficiency"),
                                                        Identifier::vanilla("sharpness"),
                                                        Identifier::vanilla("unbreaking")});
    CHECK(enchantments.contains(Identifier::vanilla("sharpness")));
    CHECK_FALSE(enchantments.contains(Identifier::vanilla("mending")));
    CHECK(enchantments.protocolIdOf(Identifier::vanilla("mending")) == std::nullopt);
    CHECK(enchantments.definitionOf(Identifier::vanilla("mending")) == nullptr);
}

TEST_CASE("a definition arrives as the NBT the registry packet carries") {
    FakeJar jar;
    jar.files[entry("sharpness")] = R"({"anvil_cost":1,"max_level":5,"weight":10,"curse":false})";
    const RealEnchantments enchantments{jar.handle()};

    const NbtCompound* definition = enchantments.definitionOf(Identifier::vanilla("sharpness"));
    REQUIRE(definition != nullptr);
    CHECK(definition->getInt("anvil_cost") == 1);
    CHECK(definition->getInt("max_level") == 5);
    CHECK(definition->getInt("weight") == 10);
    CHECK(definition->getByte("curse") == 0);
    CHECK(definition->size() == 4);
}

TEST_CASE("one entry that cannot be converted leaves no registry at all") {
    FakeJar jar;
    jar.files[entry("efficiency")] = R"({"max_level":5})";
    std::string reason;

    SUBCASE("malformed JSON") {
        jar.files[entry("sharpness")] = "{not json";
        reason = "malformed JSON";
    }
    SUBCASE("a document that is not a compound") {
        jar.files[entry("sharpness")] = "[1,2]";
        reason = "not a compound";
    }
    SUBCASE("a list that mixes types") {
        jar.files[entry("sharpness")] = R"({"levels":[1,"two"]})";
        reason = "a list mixes types";
    }
    SUBCASE("a null with no NBT equivalent") {
        jar.files[entry("sharpness")] = R"({"levels":[null]})";
        reason = "a null has no NBT equivalent";
    }

    CapturedLog log;
    const RealEnchantments enchantments{jar.handle()};

    // One warning, naming the file and exactly why it could not travel as NBT.
    REQUIRE(log.lines.size() == 1);
    CHECK(log.lines[0].find("Enchantment 'minecraft:sharpness' could not be converted (" + reason + ");") !=
          std::string::npos);

    // The good file sorts first, so a partial registry would have kept it.
    CHECK(enchantments.size() == 0);
    CHECK(enchantments.entries().empty());
    CHECK(enchantments.ids().empty());
    CHECK_FALSE(enchantments.contains(Identifier::vanilla("efficiency")));
    CHECK(enchantments.definitionOf(Identifier::vanilla("efficiency")) == nullptr);
}

TEST_CASE("an entry the jar lists but cannot read leaves no registry either") {
    FakeJar jar;
    jar.files[entry("efficiency")] = R"({"max_level":5})";
    RealEnchantments::Jar handle = jar.handle();
    handle.readText = [](std::string_view) -> std::optional<std::string> { return std::nullopt; };
    const RealEnchantments enchantments{handle};

    CHECK(enchantments.size() == 0);
}

TEST_CASE("a jar that can list but not read is refused before a single entry is read") {
    FakeJar jar;
    jar.files[entry("efficiency")] = R"({"max_level":5})";
    RealEnchantments::Jar handle = jar.handle();
    handle.readText = nullptr;
    const RealEnchantments enchantments{handle};

    CHECK(enchantments.size() == 0);
}

TEST_CASE("no jar means an empty registry rather than a missing one") {
    const RealEnchantments enchantments{RealEnchantments::Jar{}};

    CHECK(enchantments.size() == 0);
    CHECK(enchantments.ids().empty());
    CHECK(enchantments.protocolIdOf(Identifier::vanilla("sharpness")) == std::nullopt);
}
