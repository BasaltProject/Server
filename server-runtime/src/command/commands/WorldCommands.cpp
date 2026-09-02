#include <basalt/server/runtime/command/ArgumentTypes.hpp>
#include <basalt/server/runtime/command/Coordinates.hpp>
#include <basalt/server/runtime/command/commands/WorldCommands.hpp>
#include <basalt/server/runtime/player/FoodData.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/wire/protocol/play/PlayPackets.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

namespace basalt::server::runtime::command::commands {
namespace {

using player::Difficulty;
using wire::text::Component;
using world::WorldWeather;

/// The four names, in the order the packet numbers them.
constexpr std::array<std::string_view, 4> kDifficulties{"peaceful", "easy", "normal", "hard"};

[[nodiscard]] bool sameName(std::string_view left, std::string_view right) {
    return std::ranges::equal(left, right, [](char a, char b) {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(a))) ==
               static_cast<char>(std::tolower(static_cast<unsigned char>(b)));
    });
}

/// Which of the four a world is on, defaulting to peaceful for a name nobody recognises, which is
/// also what the packet would carry.
[[nodiscard]] std::int32_t difficultyOrdinal(std::string_view name) {
    const auto found =
        std::ranges::find_if(kDifficulties, [name](std::string_view known) { return sameName(known, name); });
    return found == kDifficulties.end() ? 0 : static_cast<std::int32_t>(found - kDifficulties.begin());
}

void sendToWorld(const CommandContext& context, const world::LoadedWorld& world, std::int32_t packetId,
                 const std::function<void(wire::io::ByteBuffer&)>& write) {
    for (player::PlayerSession* player : context.environment().playersIn(world)) {
        if (player->connection().sendPlayPacket) {
            player->connection().sendPlayPacket(packetId, write);
        }
    }
}

/// parity: `WeatherCommand.setClear`, `setRain` and `setThunder`, which differ only in the range
/// they sample and in which of the two countdowns they set.
[[nodiscard]] std::int32_t setWeather(const CommandContext& context, std::string_view kind,
                                      const std::int32_t* duration) {
    const Parsed<world::LoadedWorld*> world = context.world();
    if (!world.has_value()) {
        return 0;
    }
    WorldWeather& weather = (*world)->weather();
    const std::pair<std::int32_t, std::int32_t> range = kind == "clear"  ? WorldWeather::kRainDelay
                                                        : kind == "rain" ? WorldWeather::kRainDuration
                                                                         : WorldWeather::kThunderDuration;
    // Without a random there is nothing to sample, so the shortest the range allows stands in.
    random::RandomSource* random = context.environment().options().random;
    const std::int32_t ticks = duration != nullptr ? *duration
                               : random != nullptr ? WorldWeather::randomDuration(range, *random)
                                                   : range.first;
    if (kind == "clear") {
        weather.set(ticks, 0, false, false);
    } else {
        weather.set(0, ticks, true, kind == "thunder");
    }
    context.environment().syncWeather(**world);
    context.success(Component::translatable("commands.weather.set." + std::string{kind}), true);
    // parity: the result is the duration that was asked for, and -1 when none was.
    return duration != nullptr ? *duration : -1;
}

} // namespace

MsgCommand::MsgCommand() : Command("msg", permissionLevel::kAll, {"tell", "w"}) {}

void MsgCommand::build(const CommandNodeBuilder& root) const {
    root.argument("targets", playersArgument(false), [](const CommandNodeBuilder& targets) {
        targets.argument("message", messageArgument(), [](const CommandNodeBuilder& message) {
            message.executes([](const CommandContext& context) -> Parsed<std::int32_t> {
                const auto* found = context.get<std::vector<player::PlayerSession*>>("targets");
                const auto* text = context.get<std::string>("message");
                if (found == nullptr || text == nullptr) {
                    return 0;
                }
                const Component body = Component::text(*text);
                for (player::PlayerSession* target : *found) {
                    target->sendSystemMessage(Component::translatable("commands.message.display.incoming",
                                                                      {context.source().displayName(), body}));
                    context.source().sendSystemMessage(Component::translatable(
                        "commands.message.display.outgoing", {Component::text(target->name()), body}));
                }
                return static_cast<std::int32_t>(found->size());
            });
        });
    });
}

WeatherCommand::WeatherCommand() : Command("weather", permissionLevel::kGamemasters) {}

void WeatherCommand::build(const CommandNodeBuilder& root) const {
    for (const std::string_view kind : {"clear", "rain", "thunder"}) {
        root.literal(std::string{kind}, [kind](const CommandNodeBuilder& form) {
            form.executes([kind](const CommandContext& context) -> Parsed<std::int32_t> {
                return setWeather(context, kind, nullptr);
            });
            form.argument("duration", timeArgument(1), [kind](const CommandNodeBuilder& duration) {
                duration.executes([kind](const CommandContext& context) -> Parsed<std::int32_t> {
                    return setWeather(context, kind, context.get<std::int32_t>("duration"));
                });
            });
        });
    }
}

DifficultyCommand::DifficultyCommand() : Command("difficulty", permissionLevel::kGamemasters) {}

void DifficultyCommand::build(const CommandNodeBuilder& root) const {
    root.executes([](const CommandContext& context) -> Parsed<std::int32_t> {
        const Parsed<world::LoadedWorld*> world = context.world();
        if (!world.has_value()) {
            return std::unexpected{world.error()};
        }
        const std::int32_t ordinal = difficultyOrdinal((*world)->difficulty());
        context.success(Component::translatable(
            "commands.difficulty.query",
            {Component::translatable("options.difficulty." +
                                     std::string{kDifficulties[static_cast<std::size_t>(ordinal)]})}));
        return ordinal;
    });
    for (std::size_t ordinal = 0; ordinal < kDifficulties.size(); ordinal++) {
        const std::string name{kDifficulties[ordinal]};
        root.literal(name, [name, ordinal](const CommandNodeBuilder& form) {
            form.executes([name, ordinal](const CommandContext& context) -> Parsed<std::int32_t> {
                const Parsed<world::LoadedWorld*> world = context.world();
                if (!world.has_value()) {
                    return std::unexpected{world.error()};
                }
                if (sameName((*world)->difficulty(), name)) {
                    return std::unexpected{CommandSyntaxError{
                        .component = Component::translatable("commands.difficulty.failure", {Component::text(name)})}};
                }
                (*world)->setDifficulty(name);
                const wire::protocol::play::ChangeDifficultyPacket packet{
                    .difficulty = static_cast<std::uint8_t>(ordinal), .locked = (*world)->difficultyLocked()};
                sendToWorld(context, **world, wire::protocol::play::ChangeDifficultyPacket::kId,
                            [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
                context.success(Component::translatable("commands.difficulty.success",
                                                        {Component::translatable("options.difficulty." + name)}),
                                true);
                return static_cast<std::int32_t>(ordinal);
            });
        });
    }
}

SetWorldSpawnCommand::SetWorldSpawnCommand() : Command("setworldspawn", permissionLevel::kGamemasters) {}

namespace {

[[nodiscard]] std::int32_t setSpawn(const CommandContext& context, const BlockCoordinates* coordinates, float angle) {
    const Parsed<world::LoadedWorld*> world = context.world();
    if (!world.has_value()) {
        return 0;
    }
    const std::array<double, 3> from = context.source().position();
    const std::array<std::int32_t, 3> at =
        coordinates != nullptr ? coordinates->resolve(from)
                               : std::array<std::int32_t, 3>{static_cast<std::int32_t>(std::floor(from[0])),
                                                             static_cast<std::int32_t>(std::floor(from[1])),
                                                             static_cast<std::int32_t>(std::floor(from[2]))};
    (*world)->setSpawn(at[0], at[1], at[2], angle);

    const wire::protocol::play::SpawnPositionPacket packet{
        .globalPos = {.dimensionName = (*world)->levelKey(),
                      .location = wire::protocol::play::BlockPos{at[0], at[1], at[2]}},
        .yaw = angle,
        .pitch = 0.0F};
    sendToWorld(context, **world, wire::protocol::play::SpawnPositionPacket::kId,
                [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
    context.success(
        Component::translatable("commands.setworldspawn.success",
                                {Component::text(std::to_string(at[0])), Component::text(std::to_string(at[1])),
                                 Component::text(std::to_string(at[2])), Component::text(std::to_string(angle))}),
        true);
    return 1;
}

} // namespace

void SetWorldSpawnCommand::build(const CommandNodeBuilder& root) const {
    root.executes(
        [](const CommandContext& context) -> Parsed<std::int32_t> { return setSpawn(context, nullptr, 0.0F); });
    root.argument("pos", blockPosArgument(), [](const CommandNodeBuilder& pos) {
        pos.executes([](const CommandContext& context) -> Parsed<std::int32_t> {
            return setSpawn(context, context.get<BlockCoordinates>("pos"), 0.0F);
        });
        pos.argument("rotation", rotationArgument(), [](const CommandNodeBuilder& rotation) {
            rotation.executes([](const CommandContext& context) -> Parsed<std::int32_t> {
                const auto* facing = context.get<std::pair<float, float>>("rotation");
                return setSpawn(context, context.get<BlockCoordinates>("pos"),
                                facing == nullptr ? 0.0F : facing->first);
            });
        });
    });
}

} // namespace basalt::server::runtime::command::commands
