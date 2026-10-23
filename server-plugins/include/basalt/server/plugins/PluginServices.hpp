#pragma once

#include <basalt/server/plugins/PluginAccess.hpp>
#include <basalt/server/plugins/PluginAdvancements.hpp>
#include <basalt/server/plugins/PluginBossBars.hpp>
#include <basalt/server/plugins/PluginCommandSpec.hpp>
#include <basalt/server/plugins/PluginDatapacks.hpp>
#include <basalt/server/plugins/PluginEvents.hpp>
#include <basalt/server/plugins/PluginMaps.hpp>
#include <basalt/server/plugins/PluginPackets.hpp>
#include <basalt/server/plugins/PluginScoreboards.hpp>
#include <basalt/server/plugins/PluginServerControl.hpp>
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
class PluginTasks {
public:
    PluginTasks() = default;
    PluginTasks(const PluginTasks&) = delete;
    PluginTasks& operator=(const PluginTasks&) = delete;
    PluginTasks(PluginTasks&&) = delete;
    PluginTasks& operator=(PluginTasks&&) = delete;
    virtual ~PluginTasks() = default;

    [[nodiscard]] virtual std::int64_t tick() const = 0;

    virtual PluginHandle after(std::int32_t delayTicks, std::function<void()> task) = 0;

    virtual PluginHandle every(std::int32_t delayTicks, std::int32_t periodTicks, std::function<void()> task) = 0;
};

class PluginCommandCall {
public:
    using Argument = std::variant<std::monostate, bool, std::int64_t, double, std::string>;

    PluginCommandCall() = default;
    PluginCommandCall(const PluginCommandCall&) = delete;
    PluginCommandCall& operator=(const PluginCommandCall&) = delete;
    PluginCommandCall(PluginCommandCall&&) = delete;
    PluginCommandCall& operator=(PluginCommandCall&&) = delete;
    virtual ~PluginCommandCall() = default;

    [[nodiscard]] virtual std::string source() const = 0;

    [[nodiscard]] virtual bool isConsole() const = 0;

    virtual void reply(std::string_view message) = 0;

    [[nodiscard]] virtual Argument argument(std::string_view name) const = 0;
};

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

    virtual bool runAsConsole(std::string_view command) = 0;

    virtual bool runAsPlayer(PluginPlayerRef& player, std::string_view command) {
        static_cast<void>(player);
        static_cast<void>(command);
        return false;
    }
};

class PluginPaths {
public:
    PluginPaths() = default;
    PluginPaths(const PluginPaths&) = delete;
    PluginPaths& operator=(const PluginPaths&) = delete;
    PluginPaths(PluginPaths&&) = delete;
    PluginPaths& operator=(PluginPaths&&) = delete;
    virtual ~PluginPaths() = default;

    [[nodiscard]] virtual std::string packages() const = 0;

    [[nodiscard]] virtual std::string folder(std::string_view pluginId) const = 0;
};

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

    [[nodiscard]] virtual std::vector<std::pair<std::string, std::string>>
    top(std::string_view pluginId, std::string_view key, std::int32_t limit) = 0;

    [[nodiscard]] virtual std::map<std::string, std::int64_t> namespaces() { return {}; }

    virtual std::int64_t purge(std::string_view pluginId) {
        static_cast<void>(pluginId);
        return 0;
    }

    /// The NBT compound plugins keep on a holder the world does not save (kind "player", id the uuid).
    [[nodiscard]] virtual std::optional<std::vector<std::uint8_t>> holderData(std::string_view kind,
                                                                              std::string_view id) {
        static_cast<void>(kind);
        static_cast<void>(id);
        return std::nullopt;
    }

    /// An empty span forgets the holder's data.
    virtual void setHolderData(std::string_view kind, std::string_view id, std::span<const std::uint8_t> data) {
        static_cast<void>(kind);
        static_cast<void>(id);
        static_cast<void>(data);
    }
};

struct PluginFormField {
    enum class Kind : std::uint8_t { Text, Toggle, Slider, Choice };

    std::string key;
    Kind kind = Kind::Text;
    std::string label;
    std::string initialText;
    bool initialToggle = false;
    double min = 0.0;
    double max = 1.0;
    double step = 0.0;
    double initialNumber = 0.0;
    std::vector<std::string> options;
    std::string initialOption;
};

struct PluginForm {
    std::string title;
    std::vector<std::string> body;
    std::vector<PluginFormField> fields;
    std::vector<std::string> buttons;
};

/// The button pressed, from zero, and what each field held as text.
using PluginFormAnswer = std::function<void(std::size_t button, const std::map<std::string, std::string>& values)>;

class PluginMenus {
public:
    static constexpr std::int32_t kColumns = 9;
    static constexpr std::int32_t kMaxRows = 6;

    PluginMenus() = default;
    PluginMenus(const PluginMenus&) = delete;
    PluginMenus& operator=(const PluginMenus&) = delete;
    PluginMenus(PluginMenus&&) = delete;
    PluginMenus& operator=(PluginMenus&&) = delete;
    virtual ~PluginMenus() = default;

    virtual PluginHandle open(PluginPlayerRef& player, std::string_view title, std::int32_t rows,
                              const std::map<std::int32_t, PluginItem>& items,
                              std::function<void(std::int32_t)> onClick) = 0;

    virtual void close(PluginPlayerRef& player) = 0;

    virtual void refresh(PluginPlayerRef& player, const std::map<std::int32_t, PluginItem>& items) = 0;

    virtual PluginHandle openForm(PluginPlayerRef&, const PluginForm&, PluginFormAnswer) { return PluginHandle{}; }
};

class PluginPacketSender {
public:
    PluginPacketSender() = default;
    PluginPacketSender(const PluginPacketSender&) = delete;
    PluginPacketSender& operator=(const PluginPacketSender&) = delete;
    PluginPacketSender(PluginPacketSender&&) = delete;
    PluginPacketSender& operator=(PluginPacketSender&&) = delete;
    virtual ~PluginPacketSender() = default;

    /// Sends a clientbound packet; the body is in the wire format of the player's protocol, `intercept` runs listeners.
    virtual bool send(PluginPlayerRef& player, const PacketAddress& address, std::span<const std::uint8_t> bytes,
                      bool intercept) = 0;

    /// Handles a serverbound packet as if the player's client had sent it.
    virtual bool receive(PluginPlayerRef& player, const PacketAddress& address, std::span<const std::uint8_t> bytes,
                         bool intercept) = 0;

    [[nodiscard]] virtual std::optional<PacketAddress> resolve(std::string_view name) const = 0;

    /// Like `resolve`, and also the wildcard names such as `serverbound.*` or `clientbound.play.*`.
    [[nodiscard]] virtual std::optional<PacketScope> scopeOf(std::string_view name) const = 0;

    [[nodiscard]] virtual std::string nameOf(const PacketAddress& address) const = 0;

    [[nodiscard]] virtual std::vector<std::string> names() const = 0;

    /// The connection of an online Java player, or null (a Bedrock player has no Java packets).
    [[nodiscard]] virtual std::shared_ptr<PacketEndpoint> endpointOf(PluginPlayerRef& player) = 0;
};

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

    [[nodiscard]] virtual PluginPlayers& players() = 0;

    [[nodiscard]] virtual PluginWorlds& worlds() = 0;

    [[nodiscard]] virtual PluginPlaceholders& placeholders() = 0;

    [[nodiscard]] virtual PluginStorage& storage() = 0;

    [[nodiscard]] virtual PluginMenus& menus() = 0;

    [[nodiscard]] virtual PluginPacketSender& packets() = 0;

    [[nodiscard]] virtual PluginChannelSender& channels() = 0;

    virtual bool at(std::string_view world, double x, double z, std::function<void()> task) = 0;

    [[nodiscard]] virtual PluginAccess& access() { return PluginAccess::unavailable(); }

    [[nodiscard]] virtual PluginScoreboards& scoreboards() { return PluginScoreboards::unavailable(); }

    [[nodiscard]] virtual PluginBossBars& bossBars() { return PluginBossBars::unavailable(); }

    [[nodiscard]] virtual PluginAdvancements& advancements() { return PluginAdvancements::unavailable(); }

    [[nodiscard]] virtual PluginServerControl& serverControl() { return PluginServerControl::unavailable(); }

    [[nodiscard]] virtual PluginDatapacks& datapacks() { return PluginDatapacks::unavailable(); }

    [[nodiscard]] virtual PluginMaps& maps() { return PluginMaps::unavailable(); }
};
} // namespace basalt::server::plugins
