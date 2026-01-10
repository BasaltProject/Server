#include <basalt/server/runtime/command/ArgumentTypes.hpp>
#include <basalt/server/runtime/command/commands/TickCommand.hpp>
#include <basalt/wire/protocol/play/BorderPackets.hpp>

#include <cmath>
#include <cstdint>
#include <string>

namespace basalt::server::runtime::command::commands {
namespace {

namespace play = wire::protocol::play;

using wire::text::Component;

/// How far back `/tick query` averages, matching what `/timings` calls "recent".
constexpr int kQueryWindowSeconds = 60;

/// One decimal place, which is how vanilla prints every number `/tick` reports.
[[nodiscard]] std::string format(float value) {
    const auto scaled = static_cast<std::int64_t>(std::llround(value * 10.0F));
    return std::to_string(scaled / 10) + "." + std::to_string(std::abs(scaled) % 10);
}

void sendToAll(const CommandContext& context, std::int32_t packetId,
               const std::function<void(wire::io::ByteBuffer&)>& write) {
    for (const player::PlayerSession* player : context.environment().players().snapshot()) {
        if (player->connection().sendPlayPacket) {
            player->connection().sendPlayPacket(packetId, write);
        }
    }
}

} // namespace

TickCommand::TickCommand(TickLoop& tickLoop, wire::common::TickDurations& durations,
                         std::function<std::int64_t()> nowNanos)
    : Command("tick", permissionLevel::kAdmins), tickLoop_(&tickLoop), durations_(&durations),
      nowNanos_(std::move(nowNanos)) {}

void TickCommand::build(const CommandNodeBuilder& root) const {
    /// The client draws its own "frozen" overlay from this, so every change has to reach it.
    const auto announce = [this](const CommandContext& context) {
        const play::TickingStatePacket packet{.tickRate = tickLoop_->tickRate(), .frozen = tickLoop_->isFrozen()};
        sendToAll(context, play::TickingStatePacket::kId,
                  [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
    };

    root.literal("query", [this](const CommandNodeBuilder& query) {
        query.executes([this](const CommandContext& context) -> Parsed<std::int32_t> {
            const double averageMillis =
                durations_->summary(nowNanos_ ? nowNanos_() : 0, kQueryWindowSeconds).meanMillis;
            context.success(Component::translatable("commands.tick.query.rate.running",
                                                    {Component::text(format(tickLoop_->tickRate())),
                                                     Component::text(format(static_cast<float>(averageMillis))),
                                                     Component::text(format(1000.0F / tickLoop_->tickRate()))}));
            context.success(Component::translatable(tickLoop_->isSprinting() ? "commands.tick.status.sprinting"
                                                    : tickLoop_->isFrozen()  ? "commands.tick.status.frozen"
                                                                             : "commands.tick.status.running"));
            return static_cast<std::int32_t>(tickLoop_->tickRate());
        });
    });

    root.literal("rate", [this, announce](const CommandNodeBuilder& form) {
        form.argument("rate", floatArgument(TickLoop::kMinTickRate, TickLoop::kMaxTickRate),
                      [this, announce](const CommandNodeBuilder& rate) {
                          rate.executes([this, announce](const CommandContext& context) -> Parsed<std::int32_t> {
                              const auto* value = context.get<float>("rate");
                              if (value == nullptr) {
                                  return 0;
                              }
                              tickLoop_->setTickRate(*value);
                              announce(context);
                              context.success(Component::translatable("commands.tick.rate.success",
                                                                      {Component::text(format(*value))}),
                                              true);
                              return static_cast<std::int32_t>(*value);
                          });
                      });
    });

    for (const bool freezing : {true, false}) {
        root.literal(freezing ? "freeze" : "unfreeze", [this, announce, freezing](const CommandNodeBuilder& form) {
            form.executes([this, announce, freezing](const CommandContext& context) -> Parsed<std::int32_t> {
                tickLoop_->setFrozen(freezing);
                announce(context);
                context.success(
                    Component::translatable(freezing ? "commands.tick.status.frozen" : "commands.tick.status.running"),
                    true);
                return 1;
            });
        });
    }

    const auto step = [this](const CommandContext& context, std::int32_t ticks) -> Parsed<std::int32_t> {
        if (!tickLoop_->step(ticks)) {
            return std::unexpected{CommandSyntaxError{.component = Component::translatable("commands.tick.step.fail")}};
        }
        const play::TickingStepPacket packet{.steps = ticks};
        sendToAll(context, play::TickingStepPacket::kId,
                  [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        context.success(Component::translatable("commands.tick.step.success", {Component::text(std::to_string(ticks))}),
                        true);
        return ticks;
    };
    root.literal("step", [this, step](const CommandNodeBuilder& form) {
        form.executes([step](const CommandContext& context) { return step(context, 1); });
        form.literal("stop", [this](const CommandNodeBuilder& stop) {
            stop.executes([this](const CommandContext& context) -> Parsed<std::int32_t> {
                static_cast<void>(tickLoop_->step(0));
                context.success(Component::translatable("commands.tick.step.stop.success"), true);
                return 1;
            });
        });
        form.argument("time", timeArgument(1), [step](const CommandNodeBuilder& time) {
            time.executes([step](const CommandContext& context) {
                const auto* ticks = context.get<std::int32_t>("time");
                return step(context, ticks == nullptr ? 1 : *ticks);
            });
        });
    });

    root.literal("sprint", [this, announce](const CommandNodeBuilder& form) {
        form.literal("stop", [this, announce](const CommandNodeBuilder& stop) {
            stop.executes([this, announce](const CommandContext& context) -> Parsed<std::int32_t> {
                if (!tickLoop_->stopSprinting()) {
                    return std::unexpected{
                        CommandSyntaxError{.component = Component::translatable("commands.tick.sprint.stop.fail")}};
                }
                announce(context);
                context.success(Component::translatable("commands.tick.sprint.stop.success"), true);
                return 1;
            });
        });
        form.argument("time", timeArgument(1), [this, announce](const CommandNodeBuilder& time) {
            time.executes([this, announce](const CommandContext& context) -> Parsed<std::int32_t> {
                const auto* ticks = context.get<std::int32_t>("time");
                if (ticks == nullptr) {
                    return 0;
                }
                tickLoop_->sprint(*ticks);
                announce(context);
                context.success(Component::translatable("commands.tick.status.sprinting"), true);
                return *ticks;
            });
        });
    });
}

} // namespace basalt::server::runtime::command::commands
