#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/command/ArgumentTypes.hpp>
#include <basalt/server/runtime/command/CommandTarget.hpp>
#include <basalt/server/runtime/command/Coordinates.hpp>
#include <basalt/server/runtime/command/DataArguments.hpp>
#include <basalt/server/runtime/command/WorldArguments.hpp>
#include <basalt/server/runtime/command/commands/ComputeCommand.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RealLootTables.hpp>
#include <basalt/server/world/loot/LootTableJson.hpp>

#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <functional>
#include <random>
#include <string>

namespace basalt::server::runtime::command::commands {
namespace {

using wire::common::Identifier;
using wire::text::Component;

enum class Source : std::uint8_t { Default, Block, Entity };

/// parity: `LootContextSources.ContextDecorator.createParams`: `THIS_ENTITY` is the source's own
/// entity and `ORIGIN` its position; the `block` form adds the block state there.
[[nodiscard]] server::world::loot::LootContext contextOf(const CommandContext& context, world::RealLootTables* tables,
                                                         Source source) {
    server::world::loot::LootContext loot;
    const std::array<double, 3> at = context.source().position();
    if (const Parsed<world::LoadedWorld*> world = context.world(); world.has_value() && tables != nullptr) {
        tables->prepareAt(loot, **world, at[0], at[1], at[2]);
    }
    if (context.source().player() != nullptr) {
        loot.hasEntity = true;
        loot.thisEntity = server::world::loot::EntityFacts{.type = Identifier::vanilla("player")};
    }
    if (source == Source::Block) {
        const auto* coordinates = context.get<BlockCoordinates>("computePos");
        const Parsed<world::LoadedWorld*> world = context.world();
        if (coordinates != nullptr && world.has_value()) {
            const std::array<std::int32_t, 3> pos = coordinates->resolve(context.source());
            if (const std::optional<std::int32_t> state = (*world)->state().blockState(pos[0], pos[1], pos[2]);
                state.has_value()) {
                loot.blockState = server::registry::blockStateDefinitions::propertiesOf(*state);
                if (const Identifier* block = server::registry::blockStateDefinitions::blockOf(*state);
                    block != nullptr) {
                    loot.block = *block;
                }
            }
        }
    }
    return loot;
}

/// parity: `Float.toString`, the shortest digits with a ".0" on whole numbers and "E" notation
/// outside [1e-3, 1e7).
[[nodiscard]] std::string javaFloat(float value) {
    if (std::isnan(value)) {
        return "NaN";
    }
    if (std::isinf(value)) {
        return value > 0 ? "Infinity" : "-Infinity";
    }
    const float magnitude = std::fabs(value);
    const bool plain = magnitude == 0.0F || (magnitude >= 1e-3F && magnitude < 1e7F);
    std::array<char, 64> buffer{};
    const auto written = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value,
                                       plain ? std::chars_format::fixed : std::chars_format::scientific);
    std::string text{buffer.data(), written.ptr};
    if (plain) {
        return text.find('.') == std::string::npos ? text + ".0" : text;
    }
    const std::size_t e = text.find('e');
    std::string mantissa = text.substr(0, e);
    if (mantissa.find('.') == std::string::npos) {
        mantissa += ".0";
    }
    const int exponent = std::stoi(text.substr(e + 1));
    return mantissa + "E" + std::to_string(exponent);
}

/// The provider as JSON, the id standing for itself so a reference resolves through the registry.
[[nodiscard]] wire::common::JsonValue jsonOf(const ResourceOrInline& named) {
    return named.inlineValue.value_or(
        wire::common::JsonValue::of(named.id.has_value() ? named.id->toString() : std::string{}));
}

/// parity: `printExactOutput` and `printRoundedOutput`, named when the provider was.
void print(const CommandContext& context, const ResourceOrInline& named, std::int32_t result,
           std::optional<float> original) {
    std::vector<Component> arguments;
    if (named.id.has_value()) {
        arguments.push_back(Component::text(named.id->toString()));
    }
    if (original.has_value()) {
        arguments.push_back(Component::text(javaFloat(*original)));
    }
    arguments.push_back(Component::text(std::to_string(result)));
    const std::string key = std::string{"command.compute.result."} + (named.id.has_value() ? "named." : "unnamed.") +
                            (original.has_value() ? "rounded" : "exact");
    context.success(Component::translatable(key, arguments), false);
}

/// parity: `throwInvalidValue`.
[[nodiscard]] CommandSyntaxError invalid(const ResourceOrInline& named, const std::string& value) {
    if (named.id.has_value()) {
        return CommandSyntaxError{
            .component = Component::translatable("command.compute.result.named.invalid",
                                                 {Component::text(named.id->toString()), Component::text(value)})};
    }
    return CommandSyntaxError{
        .component = Component::translatable("command.compute.result.unnamed.invalid", {Component::text(value)})};
}

} // namespace

ComputeCommand::ComputeCommand(world::RealLootTables* lootTables)
    : Command("compute", permissionLevel::kGamemasters), lootTables_(lootTables) {}

void ComputeCommand::build(const CommandNodeBuilder& root) const {
    world::RealLootTables* tables = lootTables_;
    const auto integer = [tables](const CommandContext& context, Source source) -> Parsed<std::int32_t> {
        const auto* named = context.get<ResourceOrInline>("provider");
        if (named == nullptr) {
            return 0;
        }
        auto provider = server::world::loot::lootTableJson::parseIntProvider(
            jsonOf(*named), tables == nullptr ? nullptr : &tables->references());
        if (!provider.has_value()) {
            return std::unexpected{CommandSyntaxError{.component = Component::text(provider.error().message)}};
        }
        random::RandomSource random = random::RandomSource::legacy(static_cast<std::int64_t>(std::random_device{}()));
        const std::optional<std::int32_t> result = provider->tryGet(random, contextOf(context, tables, source));
        if (!result.has_value()) {
            return std::unexpected{invalid(*named, "arithmetic")};
        }
        print(context, *named, *result, std::nullopt);
        return *result;
    };
    const auto fraction = [tables](const CommandContext& context, Source source) -> Parsed<std::int32_t> {
        const auto* named = context.get<ResourceOrInline>("provider");
        if (named == nullptr) {
            return 0;
        }
        auto provider = server::world::loot::lootTableJson::parseFloatProvider(
            jsonOf(*named), tables == nullptr ? nullptr : &tables->references());
        if (!provider.has_value()) {
            return std::unexpected{CommandSyntaxError{.component = Component::text(provider.error().message)}};
        }
        random::RandomSource random = random::RandomSource::legacy(static_cast<std::int64_t>(std::random_device{}()));
        const std::optional<float> original = provider->tryGet(random, contextOf(context, tables, source));
        if (!original.has_value()) {
            return std::unexpected{invalid(*named, "arithmetic")};
        }
        if (!std::isfinite(*original)) {
            return std::unexpected{invalid(*named, javaFloat(*original))};
        }
        const auto* scale = context.get<float>("scale");
        const float factor = scale == nullptr ? 1.0F : *scale;
        const auto result = static_cast<std::int32_t>(std::floor(*original * factor));
        print(context, *named, result,
              static_cast<float>(result) == *original ? std::nullopt : std::optional<float>{*original});
        return result;
    };
    // parity: `LootContextSources.addContextSources`, the three ways into the same two forms.
    const auto forms = [integer, fraction](const CommandNodeBuilder& node, Source source) {
        node.literal("float", [fraction, source](const CommandNodeBuilder& form) {
            form.argument("provider", contextFloatProviderArgument(), [fraction, source](const CommandNodeBuilder& p) {
                p.executes([fraction, source](const CommandContext& context) { return fraction(context, source); });
                p.argument("scale", floatArgument(), [fraction, source](const CommandNodeBuilder& scale) {
                    scale.executes(
                        [fraction, source](const CommandContext& context) { return fraction(context, source); });
                });
            });
        });
        node.literal("integer", [integer, source](const CommandNodeBuilder& form) {
            form.argument("provider", contextIntProviderArgument(), [integer, source](const CommandNodeBuilder& p) {
                p.executes([integer, source](const CommandContext& context) { return integer(context, source); });
            });
        });
    };
    root.literal("default", [forms](const CommandNodeBuilder& node) { forms(node, Source::Default); });
    root.literal("block", [forms](const CommandNodeBuilder& node) {
        node.argument("computePos", blockPosArgument(),
                      [forms](const CommandNodeBuilder& pos) { forms(pos, Source::Block); });
    });
    root.literal("entity", [forms](const CommandNodeBuilder& node) {
        node.argument("computeTarget", entitiesArgument(true),
                      [forms](const CommandNodeBuilder& target) { forms(target, Source::Entity); });
    });
}

} // namespace basalt::server::runtime::command::commands
