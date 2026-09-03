#include <basalt/server/runtime/command/ArgumentTypes.hpp>
#include <basalt/server/runtime/command/CommandTarget.hpp>
#include <basalt/server/runtime/command/RegistryArguments.hpp>
#include <basalt/server/runtime/command/commands/RemainingCommands.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>

#include <cmath>
#include <sstream>
#include <string>

namespace basalt::server::runtime::command::commands {
namespace {

namespace play = wire::protocol::play;

using wire::common::Identifier;
using wire::text::Component;

/// parity: `TeamColor`'s own lowercase names, which `/waypoint modify color` reports.
constexpr std::array<std::string_view, 16> kColorNames{
    "black",     "dark_blue", "dark_green", "dark_aqua", "dark_red", "dark_purple",  "gold",   "gray",
    "dark_gray", "blue",      "green",      "aqua",      "red",      "light_purple", "yellow", "white"};

[[nodiscard]] const CommandTarget* only(const CommandContext& context) {
    const auto* matched = context.get<std::vector<CommandTarget>>("waypoint");
    return matched == nullptr || matched->size() != 1 ? nullptr : &matched->front();
}

[[nodiscard]] std::string hex(std::int32_t value) {
    std::ostringstream out;
    out << std::hex << value;
    return out.str();
}

} // namespace

WaypointCommand::WaypointCommand() : Command("waypoint", permissionLevel::kGamemasters) {}

void WaypointCommand::build(const CommandNodeBuilder& root) const {
    /// Sends the target's waypoint to everyone, which is what draws the bar entry.
    const auto broadcast = [this](const CommandContext& context) {
        const CommandTarget* target = only(context);
        if (target == nullptr) {
            return;
        }
        const auto found = icons_.find(target->entityId());
        if (found == icons_.end()) {
            return;
        }
        const std::optional<wire::common::Uuid> uuid = wire::common::Uuid::parse(target->uuid());
        if (!uuid.has_value()) {
            return;
        }
        const play::TrackedWaypointPacket packet{.operation = play::TrackedWaypointPacket::Operation::Track,
                                                 .entityUuid = *uuid,
                                                 .style = found->second.style.toString(),
                                                 .color = found->second.color,
                                                 .x = static_cast<std::int32_t>(std::floor(target->x())),
                                                 .y = static_cast<std::int32_t>(std::floor(target->y())),
                                                 .z = static_cast<std::int32_t>(std::floor(target->z()))};
        for (const player::PlayerSession* player : context.environment().players().snapshot()) {
            if (player->connection().sendPlayPacket) {
                player->connection().sendPlayPacket(play::TrackedWaypointPacket::kId,
                                                    [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
            }
        }
    };

    const auto iconOf = [this](const CommandContext& context) -> Icon* {
        const CommandTarget* target = only(context);
        return target == nullptr ? nullptr : &icons_[target->entityId()];
    };

    root.literal("list", [this](const CommandNodeBuilder& list) {
        list.executes([this](const CommandContext& context) -> Parsed<std::int32_t> {
            const Parsed<world::LoadedWorld*> world = context.world();
            if (!world.has_value()) {
                return std::unexpected{world.error()};
            }
            const auto tracked = static_cast<std::int32_t>(icons_.size());
            std::string named;
            for (const auto& [entityId, icon] : icons_) {
                named += named.empty() ? std::to_string(entityId) : ", " + std::to_string(entityId);
            }
            context.success(
                tracked == 0
                    ? Component::translatable("commands.waypoint.list.empty", {Component::text((*world)->name())})
                    : Component::translatable("commands.waypoint.list.success",
                                              {Component::text(std::to_string(tracked)),
                                               Component::text((*world)->name()), Component::text(named)}));
            return tracked;
        });
    });

    root.literal("modify", [iconOf, broadcast](const CommandNodeBuilder& modify) {
        modify.argument("waypoint", entitiesArgument(true), [iconOf, broadcast](const CommandNodeBuilder& target) {
            target.literal("color", [iconOf, broadcast](const CommandNodeBuilder& colour) {
                colour.literal("reset", [iconOf, broadcast](const CommandNodeBuilder& reset) {
                    reset.executes([iconOf, broadcast](const CommandContext& context) -> Parsed<std::int32_t> {
                        Icon* icon = iconOf(context);
                        if (icon == nullptr) {
                            return 0;
                        }
                        icon->color.reset();
                        broadcast(context);
                        context.success(Component::translatable("commands.waypoint.modify.color.reset"));
                        return 0;
                    });
                });
                colour.literal("hex", [iconOf, broadcast](const CommandNodeBuilder& form) {
                    form.argument("color", hexColorArgument(), [iconOf, broadcast](const CommandNodeBuilder& value) {
                        value.executes([iconOf, broadcast](const CommandContext& context) -> Parsed<std::int32_t> {
                            Icon* icon = iconOf(context);
                            const auto* asked = context.get<std::int32_t>("color");
                            if (icon == nullptr || asked == nullptr) {
                                return 0;
                            }
                            icon->color = *asked;
                            broadcast(context);
                            context.success(Component::translatable("commands.waypoint.modify.color",
                                                                    {Component::text(hex(*asked))}));
                            return 0;
                        });
                    });
                });
                colour.argument("color", teamColorArgument(), [iconOf, broadcast](const CommandNodeBuilder& value) {
                    value.executes([iconOf, broadcast](const CommandContext& context) -> Parsed<std::int32_t> {
                        Icon* icon = iconOf(context);
                        const auto* asked = context.get<play::TeamColor>("color");
                        if (icon == nullptr || asked == nullptr) {
                            return 0;
                        }
                        icon->color = play::teamColorRgb(*asked);
                        broadcast(context);
                        context.success(Component::translatable(
                            "commands.waypoint.modify.color",
                            {Component::text(std::string{kColorNames[static_cast<std::size_t>(*asked)]})}));
                        return 0;
                    });
                });
            });
            target.literal("style", [iconOf, broadcast](const CommandNodeBuilder& style) {
                style.literal("reset", [iconOf, broadcast](const CommandNodeBuilder& reset) {
                    reset.executes([iconOf, broadcast](const CommandContext& context) -> Parsed<std::int32_t> {
                        Icon* icon = iconOf(context);
                        if (icon == nullptr) {
                            return 0;
                        }
                        icon->style = Identifier::vanilla("default");
                        broadcast(context);
                        context.success(Component::translatable("commands.waypoint.modify.style"));
                        return 0;
                    });
                });
                style.literal("set", [iconOf, broadcast](const CommandNodeBuilder& form) {
                    form.argument(
                        "style", resourceLocationArgument(), [iconOf, broadcast](const CommandNodeBuilder& value) {
                            value.executes([iconOf, broadcast](const CommandContext& context) -> Parsed<std::int32_t> {
                                Icon* icon = iconOf(context);
                                const auto* asked = context.get<Identifier>("style");
                                if (icon == nullptr || asked == nullptr) {
                                    return 0;
                                }
                                icon->style = *asked;
                                broadcast(context);
                                context.success(Component::translatable("commands.waypoint.modify.style"));
                                return 0;
                            });
                        });
                });
            });
        });
    });
}

} // namespace basalt::server::runtime::command::commands
