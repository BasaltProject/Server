#include <basalt/server/runtime/command/ArgumentTypes.hpp>
#include <basalt/server/runtime/command/Coordinates.hpp>
#include <basalt/server/runtime/command/commands/WorldBorderCommand.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/wire/protocol/play/BorderPackets.hpp>

#include <cmath>
#include <cstdint>
#include <string>

namespace basalt::server::runtime::command::commands {
namespace {

namespace play = wire::protocol::play;

using wire::text::Component;
using world::WorldBorder;

constexpr double kMinSize = 1.0;
constexpr std::int32_t kTicksPerSecond = 20;

/// parity: vanilla prints the size with two decimals through `String.format`.
[[nodiscard]] std::string formatSize(double value) {
    const auto scaled = static_cast<std::int64_t>(std::llround(value * 100.0));
    const std::int64_t magnitude = std::abs(scaled);
    const std::string fraction = std::to_string(magnitude % 100);
    return (scaled < 0 ? "-" : "") + std::to_string(magnitude / 100) + "." +
           (fraction.size() < 2 ? "0" + fraction : fraction);
}

template <typename T>
void broadcast(const CommandContext& context, const world::LoadedWorld& world, const T& packet) {
    for (const player::PlayerSession* player : context.environment().playersIn(world)) {
        if (player->connection().sendPlayPacket) {
            player->connection().sendPlayPacket(T::kId,
                                                [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        }
    }
}

[[nodiscard]] Parsed<std::int32_t> applySize(const CommandContext& context, double newSize, std::int32_t timeTicks) {
    const Parsed<world::LoadedWorld*> world = context.world();
    if (!world.has_value()) {
        return std::unexpected{world.error()};
    }
    WorldBorder& border = (*world)->border();
    if (newSize == border.targetSize()) {
        return std::unexpected{
            CommandSyntaxError{.component = Component::translatable("commands.worldborder.set.failed.nochange")}};
    }
    if (newSize < kMinSize) {
        return std::unexpected{
            CommandSyntaxError{.component = Component::translatable("commands.worldborder.set.failed.small")}};
    }
    if (newSize > WorldBorder::kMaxSize) {
        return std::unexpected{CommandSyntaxError{
            .component = Component::translatable("commands.worldborder.set.failed.big",
                                                 {Component::text(std::to_string(WorldBorder::kMaxSize))})}};
    }
    const double old = border.targetSize();
    if (timeTicks > 0) {
        border.moveSize(newSize, timeTicks);
        broadcast(context, **world,
                  play::SetBorderLerpSizePacket{.oldSize = old, .newSize = newSize, .lerpTimeTicks = timeTicks});
        context.success(
            Component::translatable(
                newSize > old ? "commands.worldborder.set.grow" : "commands.worldborder.set.shrink",
                {Component::text(formatSize(newSize)), Component::text(std::to_string(timeTicks / kTicksPerSecond))}),
            true);
    } else {
        border.setSize(newSize);
        broadcast(context, **world, play::SetBorderSizePacket{.size = newSize});
        context.success(
            Component::translatable("commands.worldborder.set.immediate", {Component::text(formatSize(newSize))}),
            true);
    }
    return static_cast<std::int32_t>(std::abs(newSize - old));
}

template <typename T>
[[nodiscard]] T valueOf(const CommandContext& context, std::string_view name, T fallback) {
    const auto* found = context.get<T>(name);
    return found == nullptr ? fallback : *found;
}

} // namespace

WorldBorderCommand::WorldBorderCommand() : Command("worldborder", permissionLevel::kGamemasters) {}

void WorldBorderCommand::build(const CommandNodeBuilder& root) const {
    root.literal("get", [](const CommandNodeBuilder& get) {
        get.executes([](const CommandContext& context) -> Parsed<std::int32_t> {
            const Parsed<world::LoadedWorld*> world = context.world();
            if (!world.has_value()) {
                return std::unexpected{world.error()};
            }
            const double size = (*world)->border().targetSize();
            context.success(Component::translatable(
                "commands.worldborder.get", {Component::text(std::to_string(static_cast<std::int64_t>(size)))}));
            return static_cast<std::int32_t>(size);
        });
    });

    for (const bool adding : {false, true}) {
        root.literal(adding ? "add" : "set", [adding](const CommandNodeBuilder& form) {
            form.argument("distance", doubleArgument(-WorldBorder::kMaxSize, WorldBorder::kMaxSize),
                          [adding](const CommandNodeBuilder& distance) {
                              const auto apply = [adding](const CommandContext& context) -> Parsed<std::int32_t> {
                                  const Parsed<world::LoadedWorld*> world = context.world();
                                  if (!world.has_value()) {
                                      return std::unexpected{world.error()};
                                  }
                                  const double asked = valueOf<double>(context, "distance", 0.0);
                                  return applySize(context, adding ? (*world)->border().targetSize() + asked : asked,
                                                   valueOf<std::int32_t>(context, "time", 0));
                              };
                              distance.executes(apply);
                              distance.argument("time", timeArgument(0),
                                                [apply](const CommandNodeBuilder& time) { time.executes(apply); });
                          });
        });
    }

    root.literal("center", [](const CommandNodeBuilder& form) {
        form.argument("pos", vec2Argument(), [](const CommandNodeBuilder& pos) {
            pos.executes([](const CommandContext& context) -> Parsed<std::int32_t> {
                const Parsed<world::LoadedWorld*> world = context.world();
                if (!world.has_value()) {
                    return std::unexpected{world.error()};
                }
                const auto* coordinates = context.get<Coordinates>("pos");
                if (coordinates == nullptr) {
                    return 0;
                }
                const std::array<double, 3> at = coordinates->resolve(context.source());
                WorldBorder& border = (*world)->border();
                if (std::abs(at[0]) > WorldBorder::kMaxCenterCoordinate ||
                    std::abs(at[2]) > WorldBorder::kMaxCenterCoordinate) {
                    return std::unexpected{
                        CommandSyntaxError{.component = Component::translatable(
                                               "commands.worldborder.set.failed.far",
                                               {Component::text(std::to_string(WorldBorder::kMaxCenterCoordinate))})}};
                }
                if (border.centerX() == at[0] && border.centerZ() == at[2]) {
                    return std::unexpected{
                        CommandSyntaxError{.component = Component::translatable("commands.worldborder.center.failed")}};
                }
                border.center(at[0], at[2]);
                broadcast(context, **world, play::SetBorderCenterPacket{.centerX = at[0], .centerZ = at[2]});
                context.success(
                    Component::translatable("commands.worldborder.center.success",
                                            {Component::text(formatSize(at[0])), Component::text(formatSize(at[2]))}),
                    true);
                return 0;
            });
        });
    });

    root.literal("damage", [](const CommandNodeBuilder& damage) {
        const auto set = [](const CommandContext& context, std::string_view which, std::string_view argument,
                            double WorldBorder::* field) -> Parsed<std::int32_t> {
            const Parsed<world::LoadedWorld*> world = context.world();
            if (!world.has_value()) {
                return std::unexpected{world.error()};
            }
            WorldBorder& border = (*world)->border();
            const auto value = static_cast<double>(valueOf<float>(context, argument, 0.0F));
            if (border.*field == value) {
                return std::unexpected{CommandSyntaxError{
                    .component =
                        Component::translatable("commands.worldborder.damage." + std::string{which} + ".failed")}};
            }
            border.*field = value;
            context.success(Component::translatable("commands.worldborder.damage." + std::string{which} + ".success",
                                                    {Component::text(std::to_string(value))}),
                            true);
            return 0;
        };
        damage.literal("amount", [set](const CommandNodeBuilder& form) {
            form.argument("damagePerBlock", floatArgument(0.0F), [set](const CommandNodeBuilder& amount) {
                amount.executes([set](const CommandContext& context) {
                    return set(context, "amount", "damagePerBlock", &WorldBorder::damagePerBlock);
                });
            });
        });
        damage.literal("buffer", [set](const CommandNodeBuilder& form) {
            form.argument("distance", floatArgument(0.0F), [set](const CommandNodeBuilder& distance) {
                distance.executes([set](const CommandContext& context) {
                    return set(context, "buffer", "distance", &WorldBorder::damageBuffer);
                });
            });
        });
    });

    root.literal("warning", [](const CommandNodeBuilder& warning) {
        warning.literal("distance", [](const CommandNodeBuilder& form) {
            form.argument("distance", integerArgument(0), [](const CommandNodeBuilder& distance) {
                distance.executes([](const CommandContext& context) -> Parsed<std::int32_t> {
                    const Parsed<world::LoadedWorld*> world = context.world();
                    if (!world.has_value()) {
                        return std::unexpected{world.error()};
                    }
                    WorldBorder& border = (*world)->border();
                    const std::int32_t blocks = valueOf<std::int32_t>(context, "distance", 0);
                    if (border.warningBlocks == blocks) {
                        return std::unexpected{CommandSyntaxError{
                            .component = Component::translatable("commands.worldborder.warning.distance.failed")}};
                    }
                    border.warningBlocks = blocks;
                    broadcast(context, **world, play::SetBorderWarningDistancePacket{.warningBlocks = blocks});
                    context.success(Component::translatable("commands.worldborder.warning.distance.success",
                                                            {Component::text(std::to_string(blocks))}),
                                    true);
                    return 0;
                });
            });
        });
        warning.literal("time", [](const CommandNodeBuilder& form) {
            form.argument("time", timeArgument(0), [](const CommandNodeBuilder& time) {
                time.executes([](const CommandContext& context) -> Parsed<std::int32_t> {
                    const Parsed<world::LoadedWorld*> world = context.world();
                    if (!world.has_value()) {
                        return std::unexpected{world.error()};
                    }
                    WorldBorder& border = (*world)->border();
                    const std::int32_t seconds = valueOf<std::int32_t>(context, "time", 0);
                    if (border.warningSeconds == seconds) {
                        return std::unexpected{CommandSyntaxError{
                            .component = Component::translatable("commands.worldborder.warning.time.failed")}};
                    }
                    border.warningSeconds = seconds;
                    broadcast(context, **world, play::SetBorderWarningDelayPacket{.warningSeconds = seconds});
                    context.success(Component::translatable("commands.worldborder.warning.time.success",
                                                            {Component::text(std::to_string(seconds))}),
                                    true);
                    return 0;
                });
            });
        });
    });
}

} // namespace basalt::server::runtime::command::commands
