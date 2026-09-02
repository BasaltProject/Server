#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/Signs.hpp>

#include <doctest.h>

#include <string>

using namespace basalt::server::runtime::world;
using basalt::wire::common::Identifier;
using basalt::wire::nbt::NbtCompound;
using basalt::wire::nbt::NbtList;
namespace registry = basalt::server::registry;

namespace {

[[nodiscard]] std::int32_t stateOf(const std::string& path) {
    const registry::BlockState* block = registry::blocks::named(path);
    REQUIRE(block != nullptr);
    return block->stateId;
}

/// The four lines of one side, as the client would read them.
[[nodiscard]] std::vector<std::string> linesOf(const NbtCompound& data, const std::string& side) {
    const NbtCompound* text = data.getCompound(side);
    REQUIRE(text != nullptr);
    const NbtList* messages = text->getList("messages");
    REQUIRE(messages != nullptr);
    std::vector<std::string> lines;
    for (const basalt::wire::nbt::NbtTag& message : *messages) {
        const std::string* line = message.get<std::string>();
        REQUIRE(line != nullptr);
        lines.push_back(*line);
    }
    return lines;
}

} // namespace

TEST_CASE("every sign is a sign, and nothing else is") {
    CHECK(signs::isSign(stateOf("oak_sign")));
    CHECK(signs::isSign(stateOf("oak_wall_sign")));
    CHECK(signs::isSign(stateOf("oak_hanging_sign")));
    CHECK(signs::isSign(stateOf("oak_wall_hanging_sign")));
    CHECK(signs::isSign(stateOf("crimson_sign")));
    CHECK_FALSE(signs::isSign(stateOf("stone")));
    CHECK_FALSE(signs::isSign(registry::blocks::air().stateId));
}

TEST_CASE("a hanging sign is its own kind of block entity") {
    CHECK(signs::typeOf(stateOf("oak_sign")) == Identifier::vanilla("sign"));
    CHECK(signs::typeOf(stateOf("oak_wall_sign")) == Identifier::vanilla("sign"));
    // parity: the hanging ones are told apart before the plain ones, because their name ends in both.
    CHECK(signs::typeOf(stateOf("oak_hanging_sign")) == Identifier::vanilla("hanging_sign"));
    CHECK(signs::typeOf(stateOf("oak_wall_hanging_sign")) == Identifier::vanilla("hanging_sign"));
    CHECK_FALSE(signs::typeOf(stateOf("stone")).has_value());
}

TEST_CASE("a fresh sign has two blank sides and is not waxed") {
    const NbtCompound data = signs::emptyData();

    CHECK(linesOf(data, "front_text") == std::vector<std::string>{"", "", "", ""});
    CHECK(linesOf(data, "back_text") == std::vector<std::string>{"", "", "", ""});
    CHECK_FALSE(signs::isWaxed(data));

    // parity: the colour and the glow flag are always written, even on a blank side.
    const NbtCompound* front = data.getCompound("front_text");
    REQUIRE(front != nullptr);
    CHECK(front->getString("color") == "black");
    CHECK(front->getByte("has_glowing_text") == 0);
}

TEST_CASE("writing on one side leaves the other alone") {
    const NbtCompound written = signs::withText(signs::emptyData(), true, {"hello", "world"});

    CHECK(linesOf(written, "front_text") == std::vector<std::string>{"hello", "world", "", ""});
    CHECK(linesOf(written, "back_text") == std::vector<std::string>{"", "", "", ""});

    const NbtCompound both = signs::withText(written, false, {"", "", "", "back"});
    CHECK(linesOf(both, "front_text") == std::vector<std::string>{"hello", "world", "", ""});
    CHECK(linesOf(both, "back_text") == std::vector<std::string>{"", "", "", "back"});
}

TEST_CASE("a side is always four lines, however many were given") {
    CHECK(linesOf(signs::withText(signs::emptyData(), true, {}), "front_text").size() == 4);
    CHECK(linesOf(signs::withText(signs::emptyData(), true, {"a"}), "front_text").size() == 4);
    // parity: more than four is not a longer sign; the extra lines have nowhere to go.
    const std::vector<std::string> lines =
        linesOf(signs::withText(signs::emptyData(), true, {"a", "b", "c", "d", "e"}), "front_text");
    CHECK(lines == std::vector<std::string>{"a", "b", "c", "d"});
}

TEST_CASE("a line longer than a sign takes is cut to what it takes") {
    const std::string tooLong(500, 'x');

    const NbtCompound written = signs::withText(signs::emptyData(), true, {tooLong});

    // parity: `MAX_TEXT_LINE_LENGTH`, which is what the server enforces rather than the client.
    CHECK(linesOf(written, "front_text")[0].size() == 384);
    CHECK(linesOf(written, "front_text")[0] == std::string(384, 'x'));
    // One exactly that long is kept whole.
    const std::string exact(384, 'y');
    CHECK(linesOf(signs::withText(signs::emptyData(), true, {exact}), "front_text")[0] == exact);
}

TEST_CASE("writing keeps whatever else the sign was carrying") {
    NbtCompound waxed = signs::emptyData();
    waxed.putByte("is_waxed", 1);
    waxed.putString("something_else", "kept");

    const NbtCompound written = signs::withText(waxed, true, {"a"});

    CHECK(signs::isWaxed(written));
    CHECK(written.getString("something_else") == "kept");
}

TEST_CASE("a waxed sign says so") {
    NbtCompound data = signs::emptyData();
    CHECK_FALSE(signs::isWaxed(data));
    data.putByte("is_waxed", 1);
    CHECK(signs::isWaxed(data));
    // Anything that is not zero is waxed, which is how a boolean is written down.
    data.putByte("is_waxed", 2);
    CHECK(signs::isWaxed(data));
    CHECK_FALSE(signs::isWaxed(NbtCompound{}));
}

TEST_CASE("a sign block entity carries its own coordinates and an empty sign by default") {
    const basalt::server::world::ChunkBlockEntity entity = signs::blockEntity(3, 64, 5, Identifier::vanilla("sign"));

    CHECK(entity.localX() == 3);
    CHECK(entity.y() == 64);
    CHECK(entity.localZ() == 5);
    CHECK(entity.type() == Identifier::vanilla("sign"));
    CHECK(linesOf(entity.data(), "front_text") == std::vector<std::string>{"", "", "", ""});

    const basalt::server::world::ChunkBlockEntity written = signs::blockEntity(
        3, 64, 5, Identifier::vanilla("hanging_sign"), signs::withText(signs::emptyData(), true, {"hi"}));
    CHECK(linesOf(written.data(), "front_text")[0] == "hi");
}
