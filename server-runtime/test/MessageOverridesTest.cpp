#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/command/MessageOverrides.hpp>
#include <basalt/server/runtime/placeholder/PlaceholderSlots.hpp>
#include <basalt/wire/text/Serializers.hpp>

#include <doctest.h>

#include <map>
#include <string>

using basalt::server::runtime::command::MessageOverrides;
using basalt::server::runtime::player::PlayerConnection;
using basalt::server::runtime::player::PlayerSession;
using basalt::wire::common::Uuid;
using basalt::wire::text::Component;
using basalt::wire::text::LangTable;
namespace placeholder = basalt::server::runtime::placeholder;
namespace placeholders = basalt::wire::common::placeholder;

namespace {

[[nodiscard]] std::string plain(const Component& component) { return basalt::wire::text::toPlainText(component); }

} // namespace

TEST_CASE("with nothing overridden a message is handed back as it was") {
    const placeholders::PlaceholderRegistry registry;
    const MessageOverrides overrides{{}, registry, LangTable{}};
    const Component message = Component::translatable("commands.gamemode.success.self");

    CHECK(overrides.isEmpty());
    CHECK(overrides.apply(message, nullptr) == message);
}

TEST_CASE("an overridden key becomes the operator's own text") {
    const placeholders::PlaceholderRegistry registry;
    const MessageOverrides overrides{{{"commands.seed.success", "The seed is secret"}}, registry, LangTable{}};

    const Component answered =
        overrides.apply(Component::translatable("commands.seed.success", {Component::text("42")}), nullptr);

    CHECK(plain(answered) == "The seed is secret");
}

TEST_CASE("the arguments of the message it replaces are written where the operator asked") {
    const placeholders::PlaceholderRegistry registry;
    const MessageOverrides overrides{
        {{"commands.gamemode.success.other", "%1% is now playing in %2% mode"}}, registry, LangTable{}};

    const Component answered =
        overrides.apply(Component::translatable("commands.gamemode.success.other",
                                                {Component::text("Steve"), Component::text("creative")}),
                        nullptr);

    CHECK(plain(answered) == "Steve is now playing in creative mode");
}

TEST_CASE("an argument that is itself overridden is replaced before it is written in") {
    const placeholders::PlaceholderRegistry registry;
    const MessageOverrides overrides{
        {{"chat.type.admin", "[%1%: %2%]"}, {"commands.seed.success", "hidden"}}, registry, LangTable{}};

    const Component answered =
        overrides.apply(Component::translatable("chat.type.admin", {Component::text("Steve"),
                                                                    Component::translatable("commands.seed.success")}),
                        nullptr);

    CHECK(plain(answered) == "[Steve: hidden]");
}

TEST_CASE("a message that is not overridden still has its arguments looked at") {
    const placeholders::PlaceholderRegistry registry;
    const MessageOverrides overrides{{{"commands.seed.success", "hidden"}}, registry, LangTable{}};

    const Component answered =
        overrides.apply(Component::translatable("chat.type.admin", {Component::text("Steve"),
                                                                    Component::translatable("commands.seed.success")}),
                        nullptr);

    const auto* translatable = std::get_if<Component::Translatable>(&answered.payload());
    REQUIRE(translatable != nullptr);
    CHECK(translatable->key == "chat.type.admin");
    REQUIRE(translatable->args.size() == 2);
    CHECK(plain(translatable->args[1]) == "hidden");
}

TEST_CASE("a message nested under another is replaced too") {
    const placeholders::PlaceholderRegistry registry;
    const MessageOverrides overrides{{{"commands.seed.success", "hidden"}}, registry, LangTable{}};
    const Component message =
        Component::text("before ").withChildren({Component::translatable("commands.seed.success")});

    CHECK(plain(overrides.apply(message, nullptr)) == "before hidden");
}

TEST_CASE("what the operator wrote is parsed as MiniMessage") {
    const placeholders::PlaceholderRegistry registry;
    const MessageOverrides overrides{{{"commands.seed.success", "<red>secret</red>"}}, registry, LangTable{}};

    const Component answered = overrides.apply(Component::translatable("commands.seed.success"), nullptr);

    CHECK(plain(answered) == "secret");
    // The colour came from the operator's own markup rather than from the key.
    const bool coloured = answered.style().color.has_value() ||
                          (!answered.children().empty() && answered.children()[0].style().color.has_value());
    CHECK(coloured);
}

TEST_CASE("a placeholder in the operator's text is resolved against whoever ran the command") {
    placeholders::PlaceholderRegistry registry;
    registry.registerProvider("player", [](std::string_view params, const placeholders::PlaceholderContext& context) {
        const PlayerSession* player = context.get(placeholder::playerSlot());
        if (params != "name" || player == nullptr) {
            return placeholders::PlaceholderValue::unavailable("no player");
        }
        return placeholders::PlaceholderValue::of(player->name());
    });
    const MessageOverrides overrides{{{"commands.seed.success", "seed for %player_name%"}}, registry, LangTable{}};
    const PlayerSession steve{Uuid{1, 1}, "Steve", 1, PlayerConnection{}};

    CHECK(plain(overrides.apply(Component::translatable("commands.seed.success"), &steve)) == "seed for Steve");
}

TEST_CASE("the three slots are their own keys and keep their own names") {
    // A copy-paste that gave two slots one name would silently make a context answer with the
    // wrong thing, and nothing else in the build would notice.
    CHECK(placeholder::playerSlot().name() == "player");
    CHECK(placeholder::pingSlot().name() == "ping");
    CHECK(placeholder::otherPlayerSlot().name() == "other_player");
    CHECK(&placeholder::playerSlot() != &placeholder::otherPlayerSlot());

    const PlayerSession steve{Uuid{1, 1}, "Steve", 1, PlayerConnection{}};
    const placeholders::PlaceholderContext context =
        placeholders::PlaceholderContext{}.with(placeholder::playerSlot(), &steve);
    CHECK(context.get(placeholder::playerSlot()) == &steve);
    CHECK(context.get(placeholder::otherPlayerSlot()) == nullptr);
}

TEST_CASE("a placeholder nothing can answer says what is missing") {
    const placeholders::PlaceholderValue value = placeholder::notImplemented("needs the inventory system");

    CHECK_FALSE(value.isText());
    REQUIRE(value.reason.has_value());
    CHECK(*value.reason == "needs the inventory system");
}
