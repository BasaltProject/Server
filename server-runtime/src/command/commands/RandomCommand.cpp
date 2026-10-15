#include <basalt/server/runtime/command/ArgumentTypes.hpp>
#include <basalt/server/runtime/command/RegistryArguments.hpp>
#include <basalt/server/runtime/command/commands/RandomCommand.hpp>

#include <charconv>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace basalt::server::runtime::command::commands {
namespace {

using wire::common::Identifier;
using wire::text::Component;

/// Keeps the unnamed stream from repeating the world's own sequence.
constexpr std::int64_t kAnySequenceSalt = 0x5DEECE66DLL;

[[nodiscard]] std::optional<std::int32_t> toInt(std::string_view text) {
    std::int32_t value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size()) {
        return std::nullopt;
    }
    return value;
}

/// `<min>..<max>`, or a bare number, which vanilla reads as `1..n`.
[[nodiscard]] std::optional<std::pair<std::int32_t, std::int32_t>> parseRange(std::string_view text) {
    const std::size_t separator = text.find("..");
    if (separator == std::string_view::npos) {
        const std::optional<std::int32_t> single = toInt(text);
        return single.has_value() ? std::optional{std::pair{1, *single}} : std::nullopt;
    }
    const std::string_view low = text.substr(0, separator);
    const std::string_view high = text.substr(separator + 2);
    const std::int32_t min = low.empty() ? std::numeric_limits<std::int32_t>::min() : toInt(low).value_or(0);
    const std::int32_t max = high.empty() ? std::numeric_limits<std::int32_t>::max() : toInt(high).value_or(0);
    if ((!low.empty() && !toInt(low).has_value()) || (!high.empty() && !toInt(high).has_value())) {
        return std::nullopt;
    }
    return std::pair{min, max};
}

} // namespace

RandomCommand::RandomCommand(std::function<std::int64_t()> worldSeed)
    : Command("random", permissionLevel::kAll), worldSeed_(std::move(worldSeed)) {}

void RandomCommand::build(const CommandNodeBuilder& root) const {
    /// parity: `RandomSequence.createSequence`: the world seed upgraded but not mixed.
    const auto sequenceFor = [this](const Identifier& sequence) -> random::RandomSource& {
        const auto found = sequences_.find(sequence);
        if (found != sequences_.end()) {
            return found->second;
        }
        const random::Seed128bit seed = random::upgradeSeedTo128bitUnmixed(worldSeed_ ? worldSeed_() : 0)
                                            .xorWith(random::seedFromHashOf(sequence.toString()))
                                            .mixed();
        return sequences_.emplace(sequence, random::RandomSource::xoroshiro(seed)).first->second;
    };

    const auto draw = [this, sequenceFor](const CommandContext& context, std::string_view kind,
                                          const Identifier* sequence) -> Parsed<std::int32_t> {
        const auto* text = context.get<std::string>("range");
        const std::optional<std::pair<std::int32_t, std::int32_t>> range =
            text == nullptr ? std::nullopt : parseRange(*text);
        if (!range.has_value()) {
            return std::unexpected{CommandSyntaxError{
                .component = Component::text("Invalid range '" + (text == nullptr ? std::string{} : *text) + "'.")}};
        }
        const auto [min, max] = *range;
        if (min > max) {
            return std::unexpected{
                CommandSyntaxError{.component = Component::translatable("commands.random.error.range_too_small")}};
        }
        const std::int64_t span = static_cast<std::int64_t>(max) - min + 1;
        if (span > std::numeric_limits<std::int32_t>::max()) {
            return std::unexpected{
                CommandSyntaxError{.component = Component::translatable("commands.random.error.range_too_large")}};
        }
        random::RandomSource anonymous =
            random::RandomSource::legacy((worldSeed_ ? worldSeed_() : 0) ^ kAnySequenceSalt);
        random::RandomSource& random = sequence == nullptr ? anonymous : sequenceFor(*sequence);
        const std::int32_t value = min + random.nextInt(static_cast<std::int32_t>(span));
        if (kind == "roll") {
            context.environment().broadcast(Component::translatable(
                "commands.random.roll", {context.source().displayName(), Component::text(std::to_string(value)),
                                         Component::text(std::to_string(min)), Component::text(std::to_string(max))}));
        } else {
            context.success(
                Component::translatable("commands.random.sample.success", {Component::text(std::to_string(value))}));
        }
        return value;
    };

    for (const std::string_view kind : {"value", "roll"}) {
        root.literal(std::string{kind}, [kind, draw](const CommandNodeBuilder& form) {
            form.argument("range", intRangeArgument(), [kind, draw](const CommandNodeBuilder& range) {
                range.executes([kind, draw](const CommandContext& context) { return draw(context, kind, nullptr); });
                range.argument("sequence", resourceLocationArgument(), [kind, draw](const CommandNodeBuilder& named) {
                    named.executes([kind, draw](const CommandContext& context) {
                        return draw(context, kind, context.get<Identifier>("sequence"));
                    });
                });
            });
        });
    }

    const auto reset = [this](const CommandContext& context) -> Parsed<std::int32_t> {
        const auto* sequence = context.get<Identifier>("sequence");
        if (sequence == nullptr) {
            return 0;
        }
        sequences_.erase(*sequence);
        context.success(
            Component::translatable("commands.random.reset.success", {Component::text(sequence->toString())}));
        return 1;
    };
    const auto resetAll = [this](const CommandContext& context) -> Parsed<std::int32_t> {
        const auto count = static_cast<std::int32_t>(sequences_.size());
        sequences_.clear();
        context.success(
            Component::translatable("commands.random.reset.all.success", {Component::text(std::to_string(count))}));
        return count;
    };

    const auto seeded = [](const CommandNodeBuilder& node, const CommandExecutor& run) {
        node.argument("seed", integerArgument(), [run](const CommandNodeBuilder& seed) {
            seed.executes(run);
            seed.argument("includeWorldSeed", boolArgument(), [run](const CommandNodeBuilder& world) {
                world.executes(run);
                world.argument("includeSequenceId", boolArgument(),
                               [run](const CommandNodeBuilder& id) { id.executes(run); });
            });
        });
    };

    root.literal("reset", [reset, resetAll, seeded](const CommandNodeBuilder& form) {
        form.literal("*", [resetAll, seeded](const CommandNodeBuilder& all) {
            all.executes(resetAll);
            seeded(all, resetAll);
        });
        form.argument("sequence", resourceLocationArgument(), [reset, seeded](const CommandNodeBuilder& named) {
            named.executes(reset);
            seeded(named, reset);
        });
    });
}

} // namespace basalt::server::runtime::command::commands
