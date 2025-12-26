#include <basalt/server/runtime/command/ArgumentTypes.hpp>
#include <basalt/server/runtime/plugins/PacketNames.hpp>
#include <basalt/server/runtime/plugins/PluginsCommand.hpp>
#include <basalt/wire/io/FileIO.hpp>

#include <algorithm>
#include <utility>

namespace basalt::server::runtime::plugins {
namespace {

using server::plugins::PluginStatus;
using wire::text::Component;

Component grey(std::string text) { return Component::text(std::move(text)).color(wire::text::kGray); }

Component white(std::string text) { return Component::text(std::move(text)).color(wire::text::kWhite); }

Component green(std::string text) { return Component::text(std::move(text)).color(wire::text::kGreen); }

Component red(std::string text) { return Component::text(std::move(text)).color(wire::text::kRed); }

Component yellow(std::string text) { return Component::text(std::move(text)).color(wire::text::kYellow); }

std::string joined(const std::vector<std::string>& parts) {
    std::string out;
    for (const std::string& part : parts) {
        out += out.empty() ? part : ", " + part;
    }
    return out;
}

} // namespace

PluginsCommand::PluginsCommand(server::plugins::PluginHost& host,
                               std::function<std::optional<std::string>()> databasePath)
    : Command("plugins", command::permissionLevel::kAdmins), host_(&host), databasePath_(std::move(databasePath)) {}

void PluginsCommand::build(const command::CommandNodeBuilder& root) const {
    server::plugins::PluginHost* host = host_;
    root.executes([host](const command::CommandContext& context) {
        const std::vector<PluginStatus> statuses = host->statuses();
        if (statuses.empty()) {
            context.success(grey("No plugins are loaded."));
            return 0;
        }
        context.success(grey(std::to_string(statuses.size()) + " plugin(s):"));
        for (const PluginStatus& status : statuses) {
            Component state = green(" ok");
            if (status.disabled) {
                state = red(" off after " + std::to_string(status.errorStreak) + " in a row");
            } else if (status.errors > 0) {
                state = yellow(" " + std::to_string(status.errors) + " error(s)");
            }
            context.success(white("  " + status.name + " " + status.version)
                                .append(grey(" " + std::to_string(status.registrations) + " registration(s)" +
                                             (status.editable ? ", folder" : "")))
                                .append(state));
        }
        return static_cast<std::int32_t>(statuses.size());
    });

    static_cast<void>(root.literal("surfaces", [host](const command::CommandNodeBuilder& node) {
        node.executes([host](const command::CommandContext& context) {
            const std::vector<const server::plugins::PluginSurfaceClaim*> claims = host->surfaces().all();
            if (claims.empty()) {
                context.success(grey("No plugin is drawing anything."));
                return 0;
            }
            for (const server::plugins::PluginSurface surface : server::plugins::allSurfaces()) {
                std::vector<const server::plugins::PluginSurfaceClaim*> onIt;
                for (const server::plugins::PluginSurfaceClaim* claim : claims) {
                    if (claim->surface == surface) {
                        onIt.push_back(claim);
                    }
                }
                if (onIt.empty()) {
                    continue;
                }
                context.success(grey(std::string{server::plugins::surfaceName(surface)}));
                for (std::size_t index = 0; index < onIt.size(); index++) {
                    // First is whoever wins with no conditions in play; a predicate can still move
                    // it per player, and saying so is better than pretending the list is final.
                    Component standing = yellow(" waiting");
                    if (onIt[index]->yielded) {
                        standing = red(" let go");
                    } else if (index == 0) {
                        standing = green(" draws it");
                    }
                    context.success(white("  " + onIt[index]->pluginId)
                                        .append(grey(" priority " + std::to_string(onIt[index]->priority)))
                                        .append(standing)
                                        .append(onIt[index]->appliesTo ? grey(" (only sometimes)") : Component{}));
                }
            }
            return static_cast<std::int32_t>(claims.size());
        });
    }));

    static_cast<void>(root.literal("orphans", [this](const command::CommandNodeBuilder& node) {
        node.executes([this](const command::CommandContext& context) { return listOrphans(context); });
    }));

    static_cast<void>(root.literal("purge", [this](const command::CommandNodeBuilder& node) {
        static_cast<void>(
            node.argument("plugin", command::wordArgument(), [this](const command::CommandNodeBuilder& named) {
                named.executes([this](const command::CommandContext& context) { return purge(context, true); });
                static_cast<void>(named.literal("confirm", [this](const command::CommandNodeBuilder& confirmed) {
                    confirmed.executes(
                        [this](const command::CommandContext& context) { return purge(context, false); });
                }));
            }));
    }));

    static_cast<void>(root.literal("packets", [](const command::CommandNodeBuilder& node) {
        node.executes([](const command::CommandContext& context) {
            const std::vector<std::string> names = packetNames::all();
            context.success(grey(std::to_string(names.size()) + " packet(s) a plugin can name:"));
            // Grouped by direction, because that is how a plugin looks for one.
            for (const std::string_view side : {"serverbound", "clientbound"}) {
                std::vector<std::string> theirs;
                const std::string prefix = std::string{side} + ".";
                for (const std::string& name : names) {
                    if (name.starts_with(prefix)) {
                        theirs.push_back(name.substr(prefix.size()));
                    }
                }
                context.success(white("  " + std::string{side} + " (" + std::to_string(theirs.size()) + ")"));
                context.success(grey("    " + joined(theirs)));
            }
            return static_cast<std::int32_t>(names.size());
        });
    }));

    static_cast<void>(root.literal("load", [host](const command::CommandNodeBuilder& node) {
        node.executes([host](const command::CommandContext& context) {
            const server::plugins::PluginLoadReport report = host->loadAll();
            if (report.loaded.empty()) {
                context.success(grey("Nothing new to load."));
            } else {
                context.success(green("Loaded " + joined(report.loaded) + "."));
            }
            for (const auto& [what, why] : report.refused) {
                context.success(red(what + " did not load: " + why));
            }
            return static_cast<std::int32_t>(report.loaded.size());
        });
    }));

    static_cast<void>(root.literal("reload", [host](const command::CommandNodeBuilder& node) {
        static_cast<void>(
            node.argument("plugin", command::wordArgument(), [host](const command::CommandNodeBuilder& named) {
                named.executes([host](const command::CommandContext& context) {
                    const std::string* id = context.get<std::string>("plugin");
                    if (id == nullptr) {
                        return 0;
                    }
                    if (!host->isLoaded(*id)) {
                        context.success(red("'" + *id + "' is not loaded."));
                        return 0;
                    }
                    const server::plugins::PluginLoadReport report = host->reload(*id);
                    if (std::ranges::find(report.loaded, *id) != report.loaded.end()) {
                        context.success(green("Reloaded " + *id + "."));
                        return 1;
                    }
                    std::string why = "it did not come back";
                    for (const auto& [what, reason] : report.refused) {
                        if (what == *id) {
                            why = reason;
                            break;
                        }
                    }
                    context.success(red(*id + " is now unloaded: " + why));
                    return 0;
                });
            }));
    }));

    static_cast<void>(root.literal("unload", [host](const command::CommandNodeBuilder& node) {
        static_cast<void>(
            node.argument("plugin", command::wordArgument(), [host](const command::CommandNodeBuilder& named) {
                named.executes([host](const command::CommandContext& context) {
                    const std::string* id = context.get<std::string>("plugin");
                    if (id == nullptr) {
                        return 0;
                    }
                    const std::vector<std::string> gone = host->unload(*id);
                    if (gone.empty()) {
                        context.success(red("'" + *id + "' is not loaded."));
                        return 0;
                    }
                    context.success(green("Unloaded " + joined(gone) + "."));
                    return static_cast<std::int32_t>(gone.size());
                });
            }));
    }));
}

std::int32_t PluginsCommand::listOrphans(const command::CommandContext& context) const {
    const std::map<std::string, std::int64_t> orphans = host_->orphanedData();
    if (orphans.empty()) {
        context.success(grey("Every plugin with data here is installed."));
        return 0;
    }
    context.success(yellow(std::to_string(orphans.size()) + " plugin(s) left data behind:"));
    std::vector<std::pair<std::string, std::int64_t>> ordered{orphans.begin(), orphans.end()};
    std::ranges::stable_sort(ordered, [](const auto& left, const auto& right) { return left.second > right.second; });
    for (const auto& [id, rows] : ordered) {
        context.success(white("  " + id)
                            .append(grey(" " + std::to_string(rows) + " row(s)"))
                            .append(Component::text("   /plugins purge " + id).color(wire::text::kDarkGray)));
    }
    return static_cast<std::int32_t>(orphans.size());
}

std::int32_t PluginsCommand::purge(const command::CommandContext& context, bool dryRun) const {
    const std::string* id = context.get<std::string>("plugin");
    if (id == nullptr) {
        return 0;
    }
    const std::map<std::string, std::int64_t> orphans = host_->orphanedData();
    const auto found = orphans.find(*id);
    if (found == orphans.end()) {
        context.success(red("No data here belongs to '" + *id + "'. /plugins orphans lists what does."));
        return 0;
    }
    if (dryRun) {
        context.success(yellow(std::to_string(found->second) + " row(s) of '" + *id + "' would go. ")
                            .append(white("/plugins purge " + *id + " confirm"))
                            .append(yellow(" to do it.")));
        return static_cast<std::int32_t>(found->second);
    }
    const std::optional<std::string> copy = backup();
    if (!copy.has_value()) {
        context.success(red("The database could not be copied, so nothing was purged."));
        return 0;
    }
    const std::optional<std::int64_t> gone = host_->purge(*id);
    if (!gone.has_value()) {
        context.success(red("'" + *id + "' is loaded. Take it out of plugins/ first."));
        return 0;
    }
    context.success(green("Purged " + std::to_string(*gone) + " row(s) of '" + *id + "'. ")
                        .append(grey("A copy of the database is at " + *copy + ".")));
    return static_cast<std::int32_t>(*gone);
}

std::optional<std::string> PluginsCommand::backup() const {
    const std::optional<std::string> path = databasePath_ ? databasePath_() : std::nullopt;
    if (!path.has_value()) {
        return std::nullopt;
    }
    const std::optional<std::vector<std::uint8_t>> bytes = wire::io::fileIO::readBytes(*path);
    if (!bytes.has_value()) {
        return std::nullopt;
    }
    const std::string copy = *path + ".before-purge";
    return wire::io::fileIO::writeBytes(copy, *bytes) ? std::optional{copy} : std::nullopt;
}

} // namespace basalt::server::runtime::plugins
