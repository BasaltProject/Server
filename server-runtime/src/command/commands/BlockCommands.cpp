#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/command/BlockEditor.hpp>
#include <basalt/server/runtime/command/Coordinates.hpp>
#include <basalt/server/runtime/command/WorldArguments.hpp>
#include <basalt/server/runtime/command/commands/BlockCommands.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>

#include <algorithm>
#include <cstdint>
#include <string>

namespace basalt::server::runtime::command::commands {
namespace {

namespace reg = server::registry;

using wire::text::Component;

enum class Mode : std::uint8_t { Replace, Destroy, Keep, Outline, Hollow };

[[nodiscard]] std::array<std::int32_t, 3> resolve(const CommandContext& context, std::string_view name) {
    const auto* coordinates = context.get<BlockCoordinates>(name);
    return coordinates == nullptr ? std::array<std::int32_t, 3>{0, 0, 0} : coordinates->resolve(context.source());
}

[[nodiscard]] Parsed<std::int32_t> setBlock(const CommandContext& context, Mode mode, bool strict = false) {
    std::expected<BlockEditor, CommandSyntaxError> editor = blocksOf(context);
    if (!editor.has_value()) {
        return std::unexpected{editor.error()};
    }
    if (strict) {
        editor->writeStrictly();
    }
    const std::array<std::int32_t, 3> at = resolve(context, "pos");
    if (const std::optional<CommandSyntaxError> failure = editor->require(at[0], at[1], at[2]); failure.has_value()) {
        return std::unexpected{*failure};
    }
    const auto* block = context.get<BlockInput>("block");
    if (block == nullptr) {
        return 0;
    }
    if (mode == Mode::Keep && editor->stateAt(at[0], at[1], at[2]) != reg::blocks::air().stateId) {
        return std::unexpected{CommandSyntaxError{.component = Component::translatable("commands.setblock.failed")}};
    }
    if (mode == Mode::Destroy) {
        static_cast<void>(editor->destroy(at[0], at[1], at[2]));
    }
    editor->keepWritten(at[0], at[1], at[2], block->properties);
    if (!editor->set(at[0], at[1], at[2], block->stateId) && mode != Mode::Destroy) {
        return std::unexpected{CommandSyntaxError{.component = Component::translatable("commands.setblock.failed")}};
    }
    editor->flush();
    context.success(Component::translatable("commands.setblock.success", {Component::text(std::to_string(at[0])),
                                                                          Component::text(std::to_string(at[1])),
                                                                          Component::text(std::to_string(at[2]))}),
                    true);
    return 1;
}

/// parity: DESTROY breaks the old block first, so the count includes a position whose new state happens to equal the.
[[nodiscard]] bool writeOne(BlockEditor& editor, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId,
                            Mode mode) {
    if (mode == Mode::Destroy) {
        const bool cleared = editor.destroy(x, y, z);
        return editor.set(x, y, z, stateId) || cleared;
    }
    return editor.set(x, y, z, stateId);
}

[[nodiscard]] Parsed<std::int32_t> fill(const CommandContext& context, Mode mode, bool onlyAir, bool strict = false) {
    std::expected<BlockEditor, CommandSyntaxError> editor = blocksOf(context);
    if (!editor.has_value()) {
        return std::unexpected{editor.error()};
    }
    if (strict) {
        editor->writeStrictly();
    }
    const auto* filter = context.get<BlockPredicateInput>("filter");
    const std::array<std::int32_t, 3> from = resolve(context, "from");
    const std::array<std::int32_t, 3> to = resolve(context, "to");
    const std::int32_t minX = std::min(from[0], to[0]);
    const std::int32_t minY = std::min(from[1], to[1]);
    const std::int32_t minZ = std::min(from[2], to[2]);
    const std::int32_t maxX = std::max(from[0], to[0]);
    const std::int32_t maxY = std::max(from[1], to[1]);
    const std::int32_t maxZ = std::max(from[2], to[2]);

    const std::int64_t volume = static_cast<std::int64_t>(maxX - minX + 1) * (maxY - minY + 1) * (maxZ - minZ + 1);
    const std::int32_t cap = editor->world().gameRules().number(world::gameRules::kMaxBlockModifications);
    if (volume > cap) {
        return std::unexpected{CommandSyntaxError{
            .component = Component::translatable("commands.fill.toobig", {Component::text(std::to_string(cap)),
                                                                          Component::text(std::to_string(volume))})}};
    }
    if (const std::optional<CommandSyntaxError> failure = editor->require(minX, minY, minZ); failure.has_value()) {
        return std::unexpected{*failure};
    }
    if (const std::optional<CommandSyntaxError> failure = editor->require(maxX, maxY, maxZ); failure.has_value()) {
        return std::unexpected{*failure};
    }

    const auto* block = context.get<BlockInput>("block");
    if (block == nullptr) {
        return 0;
    }
    std::int32_t count = 0;
    for (std::int32_t x = minX; x <= maxX; x++) {
        for (std::int32_t y = minY; y <= maxY; y++) {
            for (std::int32_t z = minZ; z <= maxZ; z++) {
                const std::optional<std::int32_t> existing = editor->stateAt(x, y, z);
                if (!existing.has_value()) {
                    continue;
                }
                if (filter != nullptr && !filter->matches(*existing)) {
                    continue;
                }
                if (onlyAir && *existing != reg::blocks::air().stateId) {
                    continue;
                }
                const bool onEdge = x == minX || x == maxX || y == minY || y == maxY || z == minZ || z == maxZ;
                if (mode == Mode::Outline && !onEdge) {
                    continue;
                }
                const std::int32_t target =
                    mode == Mode::Hollow && !onEdge ? reg::blocks::air().stateId : block->stateId;
                if (writeOne(*editor, x, y, z, target, mode)) {
                    count++;
                }
            }
        }
    }
    editor->flush();

    if (count == 0) {
        return std::unexpected{CommandSyntaxError{.component = Component::translatable("commands.fill.failed")}};
    }
    context.success(Component::translatable("commands.fill.success", {Component::text(std::to_string(count))}), true);
    return count;
}

} // namespace

SetBlockCommand::SetBlockCommand() : Command("setblock", permissionLevel::kGamemasters) {}

void SetBlockCommand::build(const CommandNodeBuilder& root) const {
    root.argument("pos", blockPosArgument(), [](const CommandNodeBuilder& pos) {
        pos.argument("block", blockStateArgument(), [](const CommandNodeBuilder& block) {
            block.executes([](const CommandContext& context) { return setBlock(context, Mode::Replace); });
            block.literal("destroy", [](const CommandNodeBuilder& form) {
                form.executes([](const CommandContext& context) { return setBlock(context, Mode::Destroy); });
            });
            block.literal("keep", [](const CommandNodeBuilder& form) {
                form.executes([](const CommandContext& context) { return setBlock(context, Mode::Keep); });
            });
            // parity: `strict` places the state exactly as written.
            for (const auto& [word, placesExactly] :
                 std::array<std::pair<std::string_view, bool>, 2>{{{"replace", false}, {"strict", true}}}) {
                block.literal(std::string{word}, [exact = placesExactly](const CommandNodeBuilder& form) {
                    form.executes(
                        [exact](const CommandContext& context) { return setBlock(context, Mode::Replace, exact); });
                });
            }
        });
    });
}

FillCommand::FillCommand() : Command("fill", permissionLevel::kGamemasters) {}

void FillCommand::build(const CommandNodeBuilder& root) const {
    root.argument("from", blockPosArgument(), [](const CommandNodeBuilder& from) {
        from.argument("to", blockPosArgument(), [](const CommandNodeBuilder& to) {
            to.argument("block", blockStateArgument(), [](const CommandNodeBuilder& block) {
                block.executes([](const CommandContext& context) { return fill(context, Mode::Replace, false); });
                const auto forms = [](const CommandNodeBuilder& node) {
                    for (const auto& [word, mode] : std::array<std::pair<std::string_view, Mode>, 3>{
                             {{"outline", Mode::Outline}, {"hollow", Mode::Hollow}, {"destroy", Mode::Destroy}}}) {
                        node.literal(std::string{word}, [mode](const CommandNodeBuilder& form) {
                            form.executes([mode](const CommandContext& context) { return fill(context, mode, false); });
                        });
                    }
                    // parity: `FillCommand.wrapWithMode` hangs `strict` beside the three modes.
                    node.literal("strict", [](const CommandNodeBuilder& form) {
                        form.executes(
                            [](const CommandContext& context) { return fill(context, Mode::Replace, false, true); });
                    });
                };
                forms(block);
                block.literal("keep", [](const CommandNodeBuilder& form) {
                    form.executes([](const CommandContext& context) { return fill(context, Mode::Replace, true); });
                });
                block.literal("replace", [forms](const CommandNodeBuilder& replace) {
                    replace.executes([](const CommandContext& context) { return fill(context, Mode::Replace, false); });
                    replace.argument("filter", blockPredicateArgument(), [forms](const CommandNodeBuilder& filter) {
                        filter.executes(
                            [](const CommandContext& context) { return fill(context, Mode::Replace, false); });
                        forms(filter);
                    });
                });
            });
        });
    });
}

} // namespace basalt::server::runtime::command::commands
