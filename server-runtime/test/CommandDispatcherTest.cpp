#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/command/CommandDispatcher.hpp>
#include <basalt/server/runtime/command/ExecutionSource.hpp>

#include <doctest.h>

#include <memory>
#include <string>
#include <vector>

using basalt::server::runtime::command::ArgumentType;
using basalt::server::runtime::command::ArgumentValue;
using basalt::server::runtime::command::Command;
using basalt::server::runtime::command::CommandContext;
using basalt::server::runtime::command::CommandDispatcher;
using basalt::server::runtime::command::CommandEnvironment;
using basalt::server::runtime::command::CommandNode;
using basalt::server::runtime::command::CommandNodeBuilder;
using basalt::server::runtime::command::CommandSource;
using basalt::server::runtime::command::ConsoleCommandSource;
using basalt::server::runtime::command::NodeKind;
using basalt::server::runtime::command::Parsed;
using basalt::server::runtime::command::StringReader;
using basalt::server::runtime::command::SuggestionResult;
using basalt::server::runtime::player::PlayerRegistry;
using basalt::wire::protocol::play::CommandNodeData;
using basalt::wire::protocol::play::CommandsPacket;
using basalt::wire::text::Component;
namespace suggestionProviders = basalt::server::runtime::command::suggestionProviders;

namespace {

/// A whole number, which is enough of an argument to test the tree with.
class IntArgument final : public ArgumentType {
public:
    [[nodiscard]] Parsed<ArgumentValue> parse(StringReader& reader, const CommandSource&,
                                              const CommandEnvironment&) const override {
        const Parsed<std::int32_t> value = reader.readInt();
        if (!value.has_value()) {
            return std::unexpected{value.error()};
        }
        return ArgumentValue::of(*value);
    }

    [[nodiscard]] basalt::wire::protocol::play::ArgumentParser wire() const override {
        return basalt::wire::protocol::play::ArgumentParser{
            basalt::wire::protocol::play::argumentType::kBrigadierInteger};
    }
};

/// One word, with a fixed list of completions, so the suggestion walk has something to answer.
class WordArgument final : public ArgumentType {
public:
    [[nodiscard]] Parsed<ArgumentValue> parse(StringReader& reader, const CommandSource&,
                                              const CommandEnvironment&) const override {
        std::string word = reader.readUnquotedString();
        if (word.empty()) {
            return std::unexpected{reader.syntaxError("Expected a word")};
        }
        return ArgumentValue::of(std::move(word));
    }

    [[nodiscard]] basalt::wire::protocol::play::ArgumentParser wire() const override {
        return basalt::wire::protocol::play::ArgumentParser{
            basalt::wire::protocol::play::argumentType::kBrigadierString,
            basalt::wire::protocol::play::ArgumentParser::StringProperties{
                .kind = basalt::wire::protocol::play::ArgumentParser::StringKind::SingleWord}};
    }
};

/// A source that may use nothing it is not told about, so permission filtering can be seen.
class LimitedSource final : public CommandSource {
public:
    explicit LimitedSource(std::vector<std::string> granted) : granted_(std::move(granted)) {}

    [[nodiscard]] std::string name() const override { return "Limited"; }

    [[nodiscard]] Component displayName() const override { return Component::text("Limited"); }

    [[nodiscard]] std::int32_t permissionLevel() const override { return 0; }

    [[nodiscard]] std::array<double, 3> position() const override { return {0.0, 0.0, 0.0}; }

    [[nodiscard]] basalt::server::runtime::player::PlayerSession* player() const override { return nullptr; }

    void sendSystemMessage(const Component& message) const override { messages.push_back(message); }

    [[nodiscard]] bool hasPermission(std::optional<std::string_view> node) const override {
        return node.has_value() && std::ranges::find(granted_, std::string{*node}) != granted_.end();
    }

    mutable std::vector<Component> messages;

private:
    std::vector<std::string> granted_;
};

/// A command built from a lambda, so a test declares one in three lines.
class TestCommand final : public Command {
public:
    TestCommand(std::string name, std::function<void(const CommandNodeBuilder&)> build,
                std::vector<std::string> aliases = {})
        : Command(std::move(name), 0, std::move(aliases)), build_(std::move(build)) {}

    void build(const CommandNodeBuilder& root) const override { build_(root); }

private:
    std::function<void(const CommandNodeBuilder&)> build_;
};

struct Fixture {
    PlayerRegistry registry;
    CommandEnvironment environment{registry, {}};
    CommandDispatcher dispatcher{environment};
    ConsoleCommandSource console;
};

} // namespace

TEST_CASE("a command runs and answers with its own result") {
    Fixture fixture;
    std::int32_t ran = 0;
    fixture.dispatcher.registerNode("ping", [&ran](const CommandNodeBuilder& builder) {
        builder.executes([&ran](const CommandContext&) {
            ran++;
            return Parsed<std::int32_t>{7};
        });
    });

    CHECK(fixture.dispatcher.execute(fixture.console, "ping") == 7);
    CHECK(ran == 1);
}

TEST_CASE("an unknown command is reported rather than run") {
    Fixture fixture;

    CHECK(fixture.dispatcher.execute(fixture.console, "nothing") == 0);
}

TEST_CASE("an argument reaches the command already parsed") {
    Fixture fixture;
    std::int32_t seen = 0;
    const auto number = std::make_shared<IntArgument>();
    fixture.dispatcher.registerNode("give", [&seen, &number](const CommandNodeBuilder& builder) {
        builder.argument("count", number, [&seen](const CommandNodeBuilder& argument) {
            argument.executes([&seen](const CommandContext& context) {
                const std::int32_t* count = context.get<std::int32_t>("count");
                seen = count == nullptr ? -1 : *count;
                return Parsed<std::int32_t>{1};
            });
        });
    });

    CHECK(fixture.dispatcher.execute(fixture.console, "give 42") == 1);
    CHECK(seen == 42);
    // Asking for a name the tree never declared answers nothing rather than misreading it.
    CHECK(fixture.dispatcher.execute(fixture.console, "give notanumber") == 0);
}

TEST_CASE("an argument of the wrong type is not misread") {
    Fixture fixture;
    bool wrongType = false;
    const auto number = std::make_shared<IntArgument>();
    fixture.dispatcher.registerNode("give", [&wrongType, &number](const CommandNodeBuilder& builder) {
        builder.argument("count", number, [&wrongType](const CommandNodeBuilder& argument) {
            argument.executes([&wrongType](const CommandContext& context) {
                wrongType = context.get<std::string>("count") == nullptr;
                return Parsed<std::int32_t>{1};
            });
        });
    });

    static_cast<void>(fixture.dispatcher.execute(fixture.console, "give 42"));

    CHECK(wrongType);
}

TEST_CASE("a branch that fails is backtracked out of and the next one tried") {
    // parity: brigadier's own backtracking, which `/teleport <destination>` against
    // `/teleport <targets> <location>` depends on.
    Fixture fixture;
    std::string took;
    const auto word = std::make_shared<WordArgument>();
    const auto number = std::make_shared<IntArgument>();
    fixture.dispatcher.registerNode("tp", [&](const CommandNodeBuilder& builder) {
        builder.argument("destination", word, [&](const CommandNodeBuilder& destination) {
            destination.executes([&took](const CommandContext&) {
                took = "one";
                return Parsed<std::int32_t>{1};
            });
            destination.argument("x", number, [&took](const CommandNodeBuilder& coordinate) {
                coordinate.executes([&took](const CommandContext&) {
                    took = "two";
                    return Parsed<std::int32_t>{1};
                });
            });
        });
    });

    static_cast<void>(fixture.dispatcher.execute(fixture.console, "tp Steve"));
    CHECK(took == "one");

    static_cast<void>(fixture.dispatcher.execute(fixture.console, "tp Steve 10"));
    CHECK(took == "two");
}

TEST_CASE("a literal wins over an argument that would also match it") {
    Fixture fixture;
    std::string took;
    const auto word = std::make_shared<WordArgument>();
    fixture.dispatcher.registerNode("gamemode", [&](const CommandNodeBuilder& builder) {
        builder.argument("mode", word, [&took](const CommandNodeBuilder& argument) {
            argument.executes([&took](const CommandContext&) {
                took = "argument";
                return Parsed<std::int32_t>{1};
            });
        });
        builder.literal("creative", [&took](const CommandNodeBuilder& literal) {
            literal.executes([&took](const CommandContext&) {
                took = "literal";
                return Parsed<std::int32_t>{1};
            });
        });
    });

    static_cast<void>(fixture.dispatcher.execute(fixture.console, "gamemode creative"));

    CHECK(took == "literal");
}

TEST_CASE("a command switched off in the file is not registered at all") {
    PlayerRegistry registry;
    basalt::server::runtime::config::CommandsConfig commands{
        {{"stop", basalt::server::runtime::config::CommandOverride{.enabled = false}}}, {}, {}};
    CommandEnvironment::Options options;
    options.commands = [&commands]() -> const basalt::server::runtime::config::CommandsConfig& { return commands; };
    const CommandEnvironment environment{registry, std::move(options)};
    CommandDispatcher dispatcher{environment};

    const CommandNode* node =
        dispatcher.registerCommand(std::make_shared<TestCommand>("stop", [](const CommandNodeBuilder& builder) {
            builder.executes([](const CommandContext&) { return Parsed<std::int32_t>{1}; });
        }));

    CHECK(node == nullptr);
    CHECK(dispatcher.rootNode().children().empty());
}

TEST_CASE("an alias redirects onto the command rather than copying it") {
    Fixture fixture;
    std::int32_t ran = 0;
    const CommandNode* node = fixture.dispatcher.registerCommand(std::make_shared<TestCommand>(
        "teleport",
        [&ran](const CommandNodeBuilder& builder) {
            builder.executes([&ran](const CommandContext&) {
                ran++;
                return Parsed<std::int32_t>{1};
            });
        },
        std::vector<std::string>{"tp"}));

    REQUIRE(node != nullptr);
    CHECK(fixture.dispatcher.execute(fixture.console, "teleport") == 1);
    CHECK(fixture.dispatcher.execute(fixture.console, "tp") == 1);
    CHECK(ran == 2);
}

TEST_CASE("a node the source may not use is invisible rather than refused") {
    Fixture fixture;
    fixture.dispatcher.registerNode("stop", [](const CommandNodeBuilder& builder) {
        builder.permission("basalt.command.stop");
        builder.executes([](const CommandContext&) { return Parsed<std::int32_t>{1}; });
    });
    const LimitedSource nobody{{}};
    const LimitedSource operatorSource{{"basalt.command.stop"}};

    CHECK(fixture.dispatcher.execute(nobody, "stop") == 0);
    CHECK(fixture.dispatcher.execute(operatorSource, "stop") == 1);
    CHECK(fixture.dispatcher.usableCommands(nobody).empty());
    CHECK(fixture.dispatcher.usableCommands(operatorSource).size() == 1);
}

TEST_CASE("the tree a client is given holds only what that client may use") {
    Fixture fixture;
    fixture.dispatcher.registerNode("help", [](const CommandNodeBuilder& builder) {
        builder.executes([](const CommandContext&) { return Parsed<std::int32_t>{1}; });
    });
    fixture.dispatcher.registerNode("stop", [](const CommandNodeBuilder& builder) {
        builder.permission("basalt.command.stop");
        builder.executes([](const CommandContext&) { return Parsed<std::int32_t>{1}; });
    });
    const LimitedSource nobody{{}};

    const CommandsPacket forEveryone = fixture.dispatcher.treeFor(fixture.console);
    const CommandsPacket forNobody = fixture.dispatcher.treeFor(nobody);

    CHECK(forEveryone.nodes.size() == 3);
    CHECK(forNobody.nodes.size() == 2);
    CHECK(forEveryone.rootIndex == 0);
    CHECK(forEveryone.nodes[0].kind == CommandNodeData::Kind::Root);
    CHECK(forEveryone.nodes[1].executable);
}

TEST_CASE("an argument node travels with its parser and its provider name") {
    Fixture fixture;
    const auto word = std::make_shared<WordArgument>();
    fixture.dispatcher.registerNode("say", [&word](const CommandNodeBuilder& builder) {
        builder.argument("message", word, [](const CommandNodeBuilder& argument) {
            argument.suggestsFromServer([](const basalt::server::runtime::command::SuggestionContext&) {
                return std::vector<basalt::wire::protocol::play::CommandSuggestion>{};
            });
            argument.executes([](const CommandContext&) { return Parsed<std::int32_t>{1}; });
        });
    });

    const CommandsPacket packet = fixture.dispatcher.treeFor(fixture.console);

    const CommandNodeData& argument = packet.nodes[2];
    CHECK(argument.kind == CommandNodeData::Kind::Argument);
    REQUIRE(argument.parser.has_value());
    REQUIRE(argument.suggestionId.has_value());
    CHECK(*argument.suggestionId == suggestionProviders::kAskServer);
}

TEST_CASE("completions are offered for the word being typed") {
    Fixture fixture;
    fixture.dispatcher.registerNode("gamemode", [](const CommandNodeBuilder& builder) {
        builder.literal("creative", [](const CommandNodeBuilder& literal) {
            literal.executes([](const CommandContext&) { return Parsed<std::int32_t>{1}; });
        });
        builder.literal("survival", [](const CommandNodeBuilder& literal) {
            literal.executes([](const CommandContext&) { return Parsed<std::int32_t>{1}; });
        });
    });

    const SuggestionResult all = fixture.dispatcher.suggest(fixture.console, "gamemode ");
    CHECK(all.suggestions.size() == 2);
    CHECK(all.suggestions[0].text == "creative");

    const SuggestionResult narrowed = fixture.dispatcher.suggest(fixture.console, "gamemode cr");
    REQUIRE(narrowed.suggestions.size() == 1);
    CHECK(narrowed.suggestions[0].text == "creative");
    CHECK(narrowed.length == 2);
}

TEST_CASE("an argument's own provider answers, with the earlier arguments in hand") {
    Fixture fixture;
    const auto word = std::make_shared<WordArgument>();
    fixture.dispatcher.registerNode("grant", [&word](const CommandNodeBuilder& builder) {
        builder.argument("who", word, [&word](const CommandNodeBuilder& who) {
            who.argument("what", word, [](const CommandNodeBuilder& what) {
                what.suggests([](const basalt::server::runtime::command::SuggestionContext& context) {
                    const std::string* who = context.argument<std::string>("who");
                    return context.matching({who == nullptr ? "nobody" : *who + "_thing"});
                });
                what.executes([](const CommandContext&) { return Parsed<std::int32_t>{1}; });
            });
        });
    });

    const SuggestionResult result = fixture.dispatcher.suggest(fixture.console, "grant Steve ");

    REQUIRE(result.suggestions.size() == 1);
    CHECK(result.suggestions[0].text == "Steve_thing");
}

TEST_CASE("a completion for a command nobody may use is not offered") {
    Fixture fixture;
    fixture.dispatcher.registerNode("stop", [](const CommandNodeBuilder& builder) {
        builder.permission("basalt.command.stop");
        builder.executes([](const CommandContext&) { return Parsed<std::int32_t>{1}; });
    });
    const LimitedSource nobody{{}};

    CHECK(fixture.dispatcher.suggest(nobody, "st").suggestions.empty());
    CHECK(fixture.dispatcher.suggest(fixture.console, "st").suggestions.size() == 1);
}

TEST_CASE("rebuilding gives the same tree the first registration did") {
    Fixture fixture;
    fixture.dispatcher.registerCommand(std::make_shared<TestCommand>("help", [](const CommandNodeBuilder& builder) {
        builder.executes([](const CommandContext&) { return Parsed<std::int32_t>{1}; });
    }));
    const std::size_t before = fixture.dispatcher.rootNode().children().size();

    fixture.dispatcher.rebuild();

    CHECK(fixture.dispatcher.rootNode().children().size() == before);
    CHECK(fixture.dispatcher.execute(fixture.console, "help") == 1);
}

TEST_CASE("unregistering takes the command and its aliases with it") {
    Fixture fixture;
    CommandNode* node = fixture.dispatcher.registerCommand(std::make_shared<TestCommand>(
        "teleport",
        [](const CommandNodeBuilder& builder) {
            builder.executes([](const CommandContext&) { return Parsed<std::int32_t>{1}; });
        },
        std::vector<std::string>{"tp"}));
    REQUIRE(node != nullptr);
    REQUIRE(fixture.dispatcher.rootNode().children().size() == 2);

    fixture.dispatcher.unregisterNode(node);

    CHECK(fixture.dispatcher.rootNode().children().empty());
    CHECK(fixture.dispatcher.execute(fixture.console, "tp") == 0);
}

TEST_CASE("every argument that can complete is one the client will ask about") {
    // The invariant: a provider nobody will ever call is a completion the player never sees.
    Fixture fixture;
    const auto word = std::make_shared<WordArgument>();
    fixture.dispatcher.registerNode("say", [&word](const CommandNodeBuilder& builder) {
        builder.argument("message", word, [](const CommandNodeBuilder& argument) {
            argument.suggestsFromServer([](const basalt::server::runtime::command::SuggestionContext&) {
                return std::vector<basalt::wire::protocol::play::CommandSuggestion>{};
            });
            argument.executes([](const CommandContext&) { return Parsed<std::int32_t>{1}; });
        });
    });

    CHECK(fixture.dispatcher.unreachableSuggestions().empty());

    // One that answers but never says so is exactly what the report is for.
    fixture.dispatcher.registerNode("whisper", [&word](const CommandNodeBuilder& builder) {
        builder.argument("message", word, [](const CommandNodeBuilder& argument) {
            argument.suggests([](const basalt::server::runtime::command::SuggestionContext&) {
                return std::vector<basalt::wire::protocol::play::CommandSuggestion>{};
            });
            argument.executes([](const CommandContext&) { return Parsed<std::int32_t>{1}; });
        });
    });
    REQUIRE(fixture.dispatcher.unreachableSuggestions().size() == 1);
    CHECK(fixture.dispatcher.unreachableSuggestions()[0].first == "whisper message");
}

TEST_CASE("a command that needs a player says so rather than crashing") {
    Fixture fixture;
    fixture.dispatcher.registerNode("suicide", [](const CommandNodeBuilder& builder) {
        builder.executes([](const CommandContext& context) -> Parsed<std::int32_t> {
            const Parsed<basalt::server::runtime::player::PlayerSession*> player = context.requirePlayer();
            if (!player.has_value()) {
                return std::unexpected{player.error()};
            }
            return 1;
        });
    });

    CHECK(fixture.dispatcher.execute(fixture.console, "suicide") == 0);
}
