#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/world/Dialogs.hpp>
#include <basalt/wire/common/json/Json.hpp>
#include <basalt/wire/nbt/JsonToNbt.hpp>
#include <basalt/wire/text/Serializers.hpp>

#include <doctest.h>

#include <map>
#include <string>

namespace dialogs = basalt::server::runtime::world::dialogs;

using basalt::wire::common::Identifier;
using basalt::wire::nbt::NbtCompound;

namespace {

[[nodiscard]] NbtCompound of(const std::string& json) {
    const auto parsed = basalt::wire::common::parseJson(json);
    REQUIRE(parsed.has_value());
    const auto tag = basalt::wire::nbt::jsonToNbt(*parsed);
    REQUIRE(tag.has_value());
    const NbtCompound* compound = tag->get<NbtCompound>();
    REQUIRE(compound != nullptr);
    return *compound;
}

[[nodiscard]] std::string plain(const basalt::wire::text::Component& text) {
    return basalt::wire::text::toPlainText(text);
}

} // namespace

TEST_CASE("a notice becomes a title, its text and one button") {
    const NbtCompound nbt = of(R"JSON({
        "type": "minecraft:notice",
        "title": {"text": "Rules"},
        "body": [{"type": "minecraft:plain_message", "contents": {"text": "Be kind."}},
                 {"type": "minecraft:item", "item": {"id": "minecraft:stone"}}],
        "action": {"label": {"text": "Understood"},
                   "action": {"type": "minecraft:run_command", "command": "say ok"}}
    })JSON");

    const std::optional<dialogs::Dialog> made = dialogs::read(nbt, nullptr);
    REQUIRE(made.has_value());
    CHECK(plain(made->title) == "Rules");
    // The item body is dropped: a screen of buttons has nowhere to draw one.
    REQUIRE(made->body.size() == 1);
    CHECK(plain(made->body.front()) == "Be kind.");
    REQUIRE(made->buttons.size() == 1);
    CHECK(plain(made->buttons.front().label) == "Understood");
    CHECK(made->buttons.front().press == dialogs::Press::RunCommand);
    CHECK(made->buttons.front().value == "say ok");
    CHECK(!made->hasInputs);
}

TEST_CASE("a confirmation keeps both sides, and a link is text rather than a press") {
    const NbtCompound nbt = of(R"JSON({
        "type": "minecraft:confirmation",
        "title": {"text": "Leave?"},
        "yes": {"label": {"text": "Yes"}, "action": {"type": "minecraft:run_command", "command": "kick me"}},
        "no": {"label": {"text": "No"}, "action": {"type": "minecraft:open_url", "url": "https://basaltmc.org"}}
    })JSON");

    const std::optional<dialogs::Dialog> made = dialogs::read(nbt, nullptr);
    REQUIRE(made.has_value());
    REQUIRE(made->buttons.size() == 2);
    CHECK(made->buttons[0].press == dialogs::Press::RunCommand);
    CHECK(made->buttons[1].press == dialogs::Press::ShowText);
    CHECK(made->buttons[1].value == "https://basaltmc.org");
}

TEST_CASE("a list opens the dialogs it names, and stops before it follows itself forever") {
    std::map<std::string, NbtCompound> registry;
    registry["minecraft:rules"] = of(R"JSON({"type": "minecraft:notice", "title": {"text": "Rules"}})JSON");
    // One that points back at the list it is in, which is what the walk has to survive.
    registry["minecraft:loop"] = of(R"JSON({
        "type": "minecraft:dialog_list",
        "title": {"text": "Loop"},
        "dialogs": "minecraft:loop"
    })JSON");
    const dialogs::ByName byName = [&registry](const Identifier& id) -> const NbtCompound* {
        const auto found = registry.find(id.toString());
        return found == registry.end() ? nullptr : &found->second;
    };

    const NbtCompound nbt = of(R"JSON({
        "type": "minecraft:dialog_list",
        "title": {"text": "Help"},
        "dialogs": ["minecraft:rules"],
        "exit_action": {"label": {"text": "Back"}}
    })JSON");

    const std::optional<dialogs::Dialog> made = dialogs::read(nbt, byName);
    REQUIRE(made.has_value());
    REQUIRE(made->buttons.size() == 2);
    CHECK(plain(made->buttons[0].label) == "Rules");
    CHECK(made->buttons[0].press == dialogs::Press::OpenDialog);
    REQUIRE(made->buttons[0].next != nullptr);
    CHECK(plain(made->buttons[0].next->title) == "Rules");
    // The exit action has no action of its own: pressing it only closes the screen.
    CHECK(made->buttons[1].press == dialogs::Press::None);

    CHECK(dialogs::read(registry["minecraft:loop"], byName).has_value());
}

TEST_CASE("a dialog that asks for something says so, since a screen of buttons cannot") {
    const NbtCompound nbt = of(R"JSON({
        "type": "minecraft:multi_action",
        "title": {"text": "Report"},
        "inputs": [{"type": "minecraft:text", "key": "why", "label": {"text": "Why"}}],
        "actions": [{"label": {"text": "Send"},
                     "action": {"type": "minecraft:dynamic/run_command", "template": "report $(why)"}}]
    })JSON");

    const std::optional<dialogs::Dialog> made = dialogs::read(nbt, nullptr);
    REQUIRE(made.has_value());
    CHECK(made->hasInputs);
    REQUIRE(made->buttons.size() == 1);
    // A template is not a command until the answers fill it in, so it is not run blind.
    CHECK(made->buttons.front().press == dialogs::Press::None);
}
