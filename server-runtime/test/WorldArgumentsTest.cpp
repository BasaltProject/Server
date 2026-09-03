#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/VanillaBlocks.hpp>
#include <basalt/server/runtime/command/ExecutionSource.hpp>
#include <basalt/server/runtime/command/WorldArguments.hpp>

#include <doctest.h>

#include <string>

using basalt::server::runtime::command::ArgumentValue;
using basalt::server::runtime::command::BlockInput;
using basalt::server::runtime::command::BlockPredicateInput;
using basalt::server::runtime::command::CommandEnvironment;
using basalt::server::runtime::command::ConsoleCommandSource;
using basalt::server::runtime::command::ItemPredicateInput;
using basalt::server::runtime::command::Parsed;
using basalt::server::runtime::command::ParsedItem;
using basalt::server::runtime::command::StringReader;
using basalt::server::runtime::player::PlayerRegistry;
using basalt::wire::common::Identifier;
using basalt::wire::nbt::NbtCompound;
using basalt::wire::nbt::NbtTag;

namespace command = basalt::server::runtime::command;
namespace parserId = basalt::wire::protocol::play::argumentType;
namespace blocks = basalt::server::registry::blocks;

namespace {

struct Fixture {
    PlayerRegistry registry;
    CommandEnvironment environment;
    ConsoleCommandSource console;

    Fixture() : environment(registry, options()) {}

    [[nodiscard]] static CommandEnvironment::Options options() {
        CommandEnvironment::Options built;
        // One tag with two members, so a predicate has something real to match against.
        built.blockTagMembers = [](const Identifier& tag) -> std::optional<std::set<Identifier>> {
            if (tag == Identifier::vanilla("logs")) {
                return std::set<Identifier>{Identifier::vanilla("oak_log"), Identifier::vanilla("birch_log")};
            }
            return std::nullopt;
        };
        built.itemTagMembers = [](const Identifier& tag) -> std::optional<std::set<Identifier>> {
            if (tag == Identifier::vanilla("planks")) {
                return std::set<Identifier>{Identifier::vanilla("oak_planks")};
            }
            return std::nullopt;
        };
        return built;
    }

    template <typename T>
    [[nodiscard]] const T* parse(const std::shared_ptr<const command::ArgumentType>& type, const std::string& input) {
        reader = StringReader{input};
        held = type->parse(reader, console, environment);
        return held.has_value() ? held->as<T>() : nullptr;
    }

    StringReader reader{""};
    Parsed<ArgumentValue> held = std::unexpected{command::CommandSyntaxError{}};
};

} // namespace

TEST_CASE("a block state resolves to the id that goes on the wire") {
    Fixture fixture;
    const BlockInput* parsed = fixture.parse<BlockInput>(command::blockStateArgument(), "minecraft:stone");
    REQUIRE(parsed != nullptr);
    CHECK(parsed->block == Identifier::vanilla("stone"));
    CHECK(parsed->stateId == blocks::stone().stateId);
    CHECK_FALSE(parsed->isAir());

    const BlockInput* air = fixture.parse<BlockInput>(command::blockStateArgument(), "air");
    REQUIRE(air != nullptr);
    CHECK(air->isAir());
}

TEST_CASE("a property that no state carries is refused rather than placed as the default") {
    Fixture fixture;
    const BlockInput* withProperty =
        fixture.parse<BlockInput>(command::blockStateArgument(), "minecraft:oak_log[axis=y]");
    REQUIRE(withProperty != nullptr);
    CHECK(withProperty->properties.at("axis") == "y");

    CHECK(fixture.parse<BlockInput>(command::blockStateArgument(), "minecraft:oak_log[axis=diagonal]") == nullptr);
    CHECK(fixture.parse<BlockInput>(command::blockStateArgument(), "minecraft:not_a_block") == nullptr);
}

TEST_CASE("a block state refuses a tag, which only a predicate takes") {
    Fixture fixture;
    CHECK(fixture.parse<BlockInput>(command::blockStateArgument(), "#minecraft:logs") == nullptr);
}

TEST_CASE("a block predicate matches a bare block, and a tag by membership") {
    Fixture fixture;
    const BlockPredicateInput* bare =
        fixture.parse<BlockPredicateInput>(command::blockPredicateArgument(), "minecraft:stone");
    REQUIRE(bare != nullptr);
    CHECK(bare->matches(blocks::stone().stateId));
    CHECK_FALSE(bare->matches(blocks::air().stateId));

    const BlockPredicateInput* tagged =
        fixture.parse<BlockPredicateInput>(command::blockPredicateArgument(), "#minecraft:logs");
    REQUIRE(tagged != nullptr);
    const std::optional<std::int32_t> oakLog =
        basalt::server::registry::blocks::stateIdOf(Identifier::vanilla("oak_log"), {});
    REQUIRE(oakLog.has_value());
    CHECK(tagged->matches(*oakLog));
    CHECK_FALSE(tagged->matches(blocks::stone().stateId));
}

TEST_CASE("a predicate matches only on the properties that were written") {
    Fixture fixture;
    const BlockPredicateInput* parsed =
        fixture.parse<BlockPredicateInput>(command::blockPredicateArgument(), "minecraft:oak_log[axis=y]");
    REQUIRE(parsed != nullptr);
    const std::optional<std::int32_t> upright =
        basalt::server::registry::blocks::stateIdOf(Identifier::vanilla("oak_log"), {{"axis", "y"}});
    const std::optional<std::int32_t> sideways =
        basalt::server::registry::blocks::stateIdOf(Identifier::vanilla("oak_log"), {{"axis", "x"}});
    REQUIRE(upright.has_value());
    REQUIRE(sideways.has_value());
    CHECK(parsed->matches(*upright));
    CHECK_FALSE(parsed->matches(*sideways));
}

TEST_CASE("a tag nobody declared matches nothing rather than everything") {
    Fixture fixture;
    const BlockPredicateInput* parsed =
        fixture.parse<BlockPredicateInput>(command::blockPredicateArgument(), "#minecraft:nonsense");
    REQUIRE(parsed != nullptr);
    CHECK_FALSE(parsed->matches(blocks::stone().stateId));
}

TEST_CASE("an item stack takes the components written after it") {
    Fixture fixture;
    const ParsedItem* bare = fixture.parse<ParsedItem>(command::itemArgument(), "minecraft:stone");
    REQUIRE(bare != nullptr);
    CHECK(bare->id == Identifier::vanilla("stone"));
    CHECK(bare->components.empty());

    const ParsedItem* withComponents =
        fixture.parse<ParsedItem>(command::itemArgument(), "minecraft:stone[minecraft:damage=5]");
    REQUIRE(withComponents != nullptr);
    CHECK(withComponents->components.getInt("minecraft:damage") == 5);

    // A component nobody has a codec for is named rather than silently dropped.
    CHECK(fixture.parse<ParsedItem>(command::itemArgument(), "minecraft:stone[minecraft:nonsense=5]") == nullptr);
    CHECK(fixture.parse<ParsedItem>(command::itemArgument(), "minecraft:not_an_item") == nullptr);
}

TEST_CASE("an item predicate matches an id or a tag") {
    Fixture fixture;
    const ItemPredicateInput* bare =
        fixture.parse<ItemPredicateInput>(command::itemPredicateArgument(), "minecraft:stone");
    REQUIRE(bare != nullptr);
    CHECK(bare->matches("minecraft:stone"));
    CHECK_FALSE(bare->matches("minecraft:dirt"));

    const ItemPredicateInput* tagged =
        fixture.parse<ItemPredicateInput>(command::itemPredicateArgument(), "#minecraft:planks");
    REQUIRE(tagged != nullptr);
    CHECK(tagged->matches("minecraft:oak_planks"));
    CHECK_FALSE(tagged->matches("minecraft:stone"));
}

TEST_CASE("a score holder takes an invented name, which is what a fake player is") {
    Fixture fixture;
    const auto* named = fixture.parse<std::vector<std::string>>(command::scoreHolderArgument(), "#total");
    REQUIRE(named != nullptr);
    CHECK(*named == std::vector<std::string>{"#total"});

    // `*` is everybody online, which with nobody online is nobody rather than a refusal.
    const auto* everyone = fixture.parse<std::vector<std::string>>(command::scoreHolderArgument(), "*");
    REQUIRE(everyone != nullptr);
    CHECK(everyone->empty());

    CHECK(fixture.parse<std::vector<std::string>>(command::scoreHolderArgument(), " ") == nullptr);
    CHECK(command::scoreHolderArgument()->clientSuggestions().has_value());
}

TEST_CASE("an operation is one of the nine and nothing else") {
    Fixture fixture;
    for (const std::string form : {"=", "+=", "-=", "*=", "/=", "%=", "<", ">", "><"}) {
        const std::string* parsed = fixture.parse<std::string>(command::operationArgument(), form);
        REQUIRE(parsed != nullptr);
        CHECK(*parsed == form);
    }
    CHECK(fixture.parse<std::string>(command::operationArgument(), "??") == nullptr);
}

TEST_CASE("the three NBT arguments read a compound, any tag and a path") {
    Fixture fixture;
    const NbtCompound* compound = fixture.parse<NbtCompound>(command::nbtCompoundArgument(), "{a:1,b:\"two\"}");
    REQUIRE(compound != nullptr);
    CHECK(compound->getInt("a") == 1);
    // A bare value is not a compound, so the compound form refuses it.
    CHECK(fixture.parse<NbtCompound>(command::nbtCompoundArgument(), "5") == nullptr);

    const NbtTag* tag = fixture.parse<NbtTag>(command::nbtTagArgument(), "5");
    REQUIRE(tag != nullptr);
    CHECK(tag->get<std::int32_t>() != nullptr);

    // A quoted section may hold a space, so the path is not split on the first one.
    const std::string* path =
        fixture.parse<std::string>(command::nbtPathArgument(), "crew[{name:\"bo peep\"}].age rest");
    REQUIRE(path != nullptr);
    CHECK(*path == "crew[{name:\"bo peep\"}].age");
    CHECK(fixture.parse<std::string>(command::nbtPathArgument(), " ") == nullptr);
}

TEST_CASE("each argument declares the parser the client validates with") {
    CHECK(command::blockStateArgument()->wire().registryId() == parserId::kBlockState);
    CHECK(command::blockPredicateArgument()->wire().registryId() == parserId::kBlockPredicate);
    CHECK(command::itemArgument()->wire().registryId() == parserId::kItemStack);
    CHECK(command::itemPredicateArgument()->wire().registryId() == parserId::kItemPredicate);
    CHECK(command::scoreHolderArgument()->wire().registryId() == parserId::kScoreHolder);
    CHECK(command::operationArgument()->wire().registryId() == parserId::kOperation);
    CHECK(command::nbtCompoundArgument()->wire().registryId() == parserId::kNbtCompoundTag);
    CHECK(command::nbtTagArgument()->wire().registryId() == parserId::kNbtTag);
    CHECK(command::nbtPathArgument()->wire().registryId() == parserId::kNbtPath);
}
