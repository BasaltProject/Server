#pragma once

#include <basalt/server/plugins/PluginCommandSpec.hpp>
#include <basalt/server/plugins/PluginEvents.hpp>
#include <basalt/server/plugins/PluginWorldAccess.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace basalt::server::plugins {

/// When plugin work runs.
///
/// Everything here happens on an owner that is allowed to run plugins, so a plugin never has to ask
/// which thread it is on: it is always on one that may touch what it is about to touch.
class PluginTasks {
public:
    PluginTasks() = default;
    PluginTasks(const PluginTasks&) = delete;
    PluginTasks& operator=(const PluginTasks&) = delete;
    PluginTasks(PluginTasks&&) = delete;
    PluginTasks& operator=(PluginTasks&&) = delete;
    virtual ~PluginTasks() = default;

    /// Ticks since the server started, for a plugin that needs to measure rather than wait.
    [[nodiscard]] virtual std::int64_t tick() const = 0;

    virtual PluginHandle after(std::int32_t delayTicks, std::function<void()> task) = 0;

    virtual PluginHandle every(std::int32_t delayTicks, std::int32_t periodTicks, std::function<void()> task) = 0;
};

/// One run of a plugin command.
class PluginCommandCall {
public:
    /// A parsed argument, in the shapes a command spec can produce.
    using Argument = std::variant<std::monostate, bool, std::int64_t, double, std::string>;

    PluginCommandCall() = default;
    PluginCommandCall(const PluginCommandCall&) = delete;
    PluginCommandCall& operator=(const PluginCommandCall&) = delete;
    PluginCommandCall(PluginCommandCall&&) = delete;
    PluginCommandCall& operator=(PluginCommandCall&&) = delete;
    virtual ~PluginCommandCall() = default;

    /// Who ran it: a player's name, or the console's.
    [[nodiscard]] virtual std::string source() const = 0;

    [[nodiscard]] virtual bool isConsole() const = 0;

    /// Answers in MiniMessage, which is the only text format the plugin API takes.
    virtual void reply(std::string_view message) = 0;

    /// The parsed value, empty when an optional argument was left out.
    [[nodiscard]] virtual Argument argument(std::string_view name) const = 0;
};

/// How a plugin's commands reach the server's own dispatcher, rather than a second one beside it.
class PluginCommands {
public:
    PluginCommands() = default;
    PluginCommands(const PluginCommands&) = delete;
    PluginCommands& operator=(const PluginCommands&) = delete;
    PluginCommands(PluginCommands&&) = delete;
    PluginCommands& operator=(PluginCommands&&) = delete;
    virtual ~PluginCommands() = default;

    virtual PluginHandle registerCommand(std::string_view pluginId, const PluginCommandSpec& spec,
                                         std::function<void(PluginCommandCall&)> run) = 0;

    /// Runs one as the console, for a plugin the operator granted `commands`. Answers whether it ran.
    virtual bool runAsConsole(std::string_view command) = 0;
};

/// Where the server keeps what belongs to plugins.
///
/// One folder each, `plugins/<id>/`, and what is inside it is the plugin's business. The server does
/// not impose `config/`, `data/` or `lang/`: most plugins want none of that, and the ones that do
/// want it their own way.
class PluginPaths {
public:
    PluginPaths() = default;
    PluginPaths(const PluginPaths&) = delete;
    PluginPaths& operator=(const PluginPaths&) = delete;
    PluginPaths(PluginPaths&&) = delete;
    PluginPaths& operator=(PluginPaths&&) = delete;
    virtual ~PluginPaths() = default;

    /// Where the packages and the folders being worked on live.
    [[nodiscard]] virtual std::string packages() const = 0;

    /// One plugin's folder: its configuration, its data, and whatever else it decided to have.
    [[nodiscard]] virtual std::string folder(std::string_view pluginId) const = 0;
};

/// What a plugin keeps: about a player, and about itself.
///
/// Namespaced by the plugin's id at this seam rather than inside the plugin, so there is no call a
/// plugin can make that names somebody else's namespace. It cannot reach the server's own tables at
/// all: this is the only door, and it has one room behind it.
class PluginStorage {
public:
    PluginStorage() = default;
    PluginStorage(const PluginStorage&) = delete;
    PluginStorage& operator=(const PluginStorage&) = delete;
    PluginStorage(PluginStorage&&) = delete;
    PluginStorage& operator=(PluginStorage&&) = delete;
    virtual ~PluginStorage() = default;

    [[nodiscard]] virtual std::optional<std::string> playerValue(std::string_view pluginId, std::string_view uuid,
                                                                 std::string_view key) = 0;

    virtual void setPlayerValue(std::string_view pluginId, std::string_view uuid, std::string_view key,
                                const std::optional<std::string>& value) = 0;

    [[nodiscard]] virtual std::map<std::string, std::string> playerValues(std::string_view pluginId,
                                                                          std::string_view uuid) = 0;

    /// The highest `key` across every player this plugin knows: what a file cannot answer.
    [[nodiscard]] virtual std::vector<std::pair<std::string, std::string>>
    top(std::string_view pluginId, std::string_view key, std::int32_t limit) = 0;

    /// Which plugins have rows here and how many, for the orphan check at startup.
    ///
    /// Not part of what a plugin can call: this is the server looking at its own database, and it
    /// names every plugin, which is exactly what no plugin may see.
    [[nodiscard]] virtual std::map<std::string, std::int64_t> namespaces() { return {}; }

    /// Deletes everything one plugin kept. Only `/plugins purge` calls this, and only when asked.
    virtual std::int64_t purge(std::string_view pluginId) {
        static_cast<void>(pluginId);
        return 0;
    }
};

/// A window a plugin draws and a player clicks, with none of the slot arithmetic.
///
/// The reason this exists rather than leaving plugins to build one out of a chest is that everyone
/// writes the same four bugs doing that: the item is taken because the click was not cancelled, the
/// numbers are off because the player's own inventory shares the window's numbering, the menu
/// survives a reload, and two menus of the same plugin get confused for each other.
class PluginMenus {
public:
    /// A window is rows of nine, and the client draws at most six of them.
    static constexpr std::int32_t kColumns = 9;
    static constexpr std::int32_t kMaxRows = 6;

    PluginMenus() = default;
    PluginMenus(const PluginMenus&) = delete;
    PluginMenus& operator=(const PluginMenus&) = delete;
    PluginMenus(PluginMenus&&) = delete;
    PluginMenus& operator=(PluginMenus&&) = delete;
    virtual ~PluginMenus() = default;

    /// Shows `player` a window of `rows` rows of nine, filled from `items` by slot.
    ///
    /// Nothing in it can be taken: a menu is a set of buttons, and a plugin that wants a container
    /// people put things into wants a chest, which is a different thing.
    virtual PluginHandle open(PluginPlayerRef& player, std::string_view title, std::int32_t rows,
                              const std::map<std::int32_t, PluginItem>& items,
                              std::function<void(std::int32_t)> onClick) = 0;

    /// Shuts whatever this player has open, as if they pressed escape.
    virtual void close(PluginPlayerRef& player) = 0;

    /// Redraws an open menu after its items changed.
    virtual void refresh(PluginPlayerRef& player, const std::map<std::int32_t, PluginItem>& items) = 0;
};

/// Sending a packet that the rest of the API does not cover.
class PluginPacketSender {
public:
    PluginPacketSender() = default;
    PluginPacketSender(const PluginPacketSender&) = delete;
    PluginPacketSender& operator=(const PluginPacketSender&) = delete;
    PluginPacketSender(PluginPacketSender&&) = delete;
    PluginPacketSender& operator=(PluginPacketSender&&) = delete;
    virtual ~PluginPacketSender() = default;

    /// Answers false if this build has no packet by that name.
    virtual bool send(PluginPlayerRef& player, std::string_view name, std::span<const std::uint8_t> bytes) = 0;

    [[nodiscard]] virtual bool isKnown(std::string_view name) const = 0;

    [[nodiscard]] virtual std::vector<std::string> names() const = 0;
};

/// Talking to the client, or to whatever is in front of it.
///
/// Not one of the ways plugins talk to each other: a channel goes out of this server, to a mod on
/// the client or to the proxy, and that is why it carries bytes and not values.
class PluginChannelSender {
public:
    PluginChannelSender() = default;
    PluginChannelSender(const PluginChannelSender&) = delete;
    PluginChannelSender& operator=(const PluginChannelSender&) = delete;
    PluginChannelSender(PluginChannelSender&&) = delete;
    PluginChannelSender& operator=(PluginChannelSender&&) = delete;
    virtual ~PluginChannelSender() = default;

    virtual bool send(PluginPlayerRef& player, std::string_view channel, std::span<const std::uint8_t> bytes) = 0;
};

/// Everything the host borrows from the server. The server implements it; the host never reaches
/// back.
class PluginServices {
public:
    PluginServices() = default;
    PluginServices(const PluginServices&) = delete;
    PluginServices& operator=(const PluginServices&) = delete;
    PluginServices(PluginServices&&) = delete;
    PluginServices& operator=(PluginServices&&) = delete;
    virtual ~PluginServices() = default;

    [[nodiscard]] virtual PluginTasks& tasks() = 0;
    [[nodiscard]] virtual PluginCommands& commands() = 0;
    [[nodiscard]] virtual PluginPaths& paths() = 0;

    /// Who is connected.
    [[nodiscard]] virtual PluginPlayers& players() = 0;

    /// The worlds and their blocks.
    [[nodiscard]] virtual PluginWorlds& worlds() = 0;

    /// The server's own placeholder registry, which a plugin adds to rather than replacing.
    [[nodiscard]] virtual PluginPlaceholders& placeholders() = 0;

    /// Where a plugin's own data goes, namespaced by its id.
    [[nodiscard]] virtual PluginStorage& storage() = 0;

    /// Windows a plugin draws.
    [[nodiscard]] virtual PluginMenus& menus() = 0;

    /// Sending a packet the rest of the API does not cover, and the names of the ones there are.
    [[nodiscard]] virtual PluginPacketSender& packets() = 0;

    /// Talking to a mod on the client, or to the proxy.
    [[nodiscard]] virtual PluginChannelSender& channels() = 0;

    /// Runs `task` on the owner of a position, and answers whether it could be arranged.
    ///
    /// Reaching a block that belongs to another owner is a task for that owner rather than a lock
    /// taken here. With regions off it is the same owner, and the whole thing is one queue entry.
    virtual bool at(std::string_view world, double x, double z, std::function<void()> task) = 0;
};

} // namespace basalt::server::plugins
