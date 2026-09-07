#include <basalt/server/runtime/command/ArgumentTypes.hpp>
#include <basalt/server/runtime/command/Coordinates.hpp>
#include <basalt/server/runtime/command/ParticleArgument.hpp>
#include <basalt/server/runtime/command/commands/ParticleCommand.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace basalt::server::runtime::command::commands {
namespace {

namespace play = wire::protocol::play;

using wire::text::Component;

} // namespace

ParticleCommand::ParticleCommand() : Command("particle", permissionLevel::kGamemasters) {}

void ParticleCommand::build(const CommandNodeBuilder& root) const {
    const auto particle = [](const CommandContext& context, bool positioned, bool spread, bool force,
                             bool named) -> Parsed<std::int32_t> {
        const auto* parsed = context.get<ParsedParticle>("name");
        if (parsed == nullptr || parsed->type == nullptr) {
            return 0;
        }
        const Parsed<world::LoadedWorld*> world = context.world();
        if (!world.has_value()) {
            return std::unexpected{world.error()};
        }
        const auto* position = positioned ? context.get<Coordinates>("pos") : nullptr;
        const std::array<double, 3> at =
            position == nullptr ? context.source().position() : position->resolve(context.source().position());
        const auto* delta = spread ? context.get<Coordinates>("delta") : nullptr;
        const std::array<double, 3> offset =
            delta == nullptr ? std::array<double, 3>{0.0, 0.0, 0.0} : delta->resolve({0.0, 0.0, 0.0});
        const auto* speed = spread ? context.get<float>("speed") : nullptr;
        const auto* count = spread ? context.get<std::int32_t>("count") : nullptr;

        const play::LevelParticlesPacket packet{.particleTypeId = parsed->type->protocolId,
                                                .x = at[0],
                                                .y = at[1],
                                                .z = at[2],
                                                .offsetX = static_cast<float>(offset[0]),
                                                .offsetY = static_cast<float>(offset[1]),
                                                .offsetZ = static_cast<float>(offset[2]),
                                                .maxSpeed = speed == nullptr ? 0.0F : *speed,
                                                .count = count == nullptr ? 1 : *count,
                                                .overrideLimiter = force,
                                                .options = parsed->options};
        const auto* viewers = named ? context.get<std::vector<player::PlayerSession*>>("viewers") : nullptr;
        const std::vector<player::PlayerSession*> audience =
            viewers != nullptr ? *viewers : context.environment().playersIn(**world);
        if (audience.empty()) {
            return std::unexpected{
                CommandSyntaxError{.component = Component::translatable("commands.particle.failed")}};
        }
        for (const player::PlayerSession* player : audience) {
            if (player->connection().sendPlayPacket) {
                player->connection().sendPlayPacket(play::LevelParticlesPacket::kId,
                                                    [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
            }
        }
        context.success(
            Component::translatable("commands.particle.success", {Component::text(parsed->type->id.toString())}));
        return static_cast<std::int32_t>(audience.size());
    };

    root.argument("name", particleArgument(), [particle](const CommandNodeBuilder& name) {
        name.executes(
            [particle](const CommandContext& context) { return particle(context, false, false, false, false); });
        name.argument("pos", vec3Argument(), [particle](const CommandNodeBuilder& pos) {
            pos.executes(
                [particle](const CommandContext& context) { return particle(context, true, false, false, false); });
            pos.argument("delta", vec3Argument(), [particle](const CommandNodeBuilder& delta) {
                delta.argument("speed", floatArgument(0.0F), [particle](const CommandNodeBuilder& speed) {
                    speed.argument("count", integerArgument(0), [particle](const CommandNodeBuilder& count) {
                        count.executes([particle](const CommandContext& context) {
                            return particle(context, true, true, false, false);
                        });
                        for (const bool force : {true, false}) {
                            count.literal(force ? "force" : "normal", [particle,
                                                                       force](const CommandNodeBuilder& mode) {
                                mode.executes([particle, force](const CommandContext& context) {
                                    return particle(context, true, true, force, false);
                                });
                                mode.argument("viewers", playersArgument(false),
                                              [particle, force](const CommandNodeBuilder& viewers) {
                                                  viewers.executes([particle, force](const CommandContext& context) {
                                                      return particle(context, true, true, force, true);
                                                  });
                                              });
                            });
                        }
                    });
                });
            });
        });
    });
}

} // namespace basalt::server::runtime::command::commands
