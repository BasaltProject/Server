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

    virtual bool send(PluginPlayerRef& player, std::string_view name, std::span<const std::uint8_t> bytes) = 0;

    [[nodiscard]] virtual bool isKnown(std::string_view name) const = 0;

    [[nodiscard]] virtual std::vector<std::string> names() const = 0;
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
};
} // namespace basalt::server::plugins
