#pragma once

#include <basalt/server/runtime/chat/ChatMessage.hpp>
#include <basalt/server/runtime/config/GroupsConfig.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/wire/common/logging/Logger.hpp>
#include <basalt/wire/text/Component.hpp>
#include <basalt/wire/text/LangTable.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace basalt::server::runtime::command {

class ExecutionSource;

namespace permissionLevel {

inline constexpr std::int32_t kAll = 0;
inline constexpr std::int32_t kModerators = 1;
inline constexpr std::int32_t kGamemasters = 2;
inline constexpr std::int32_t kAdmins = 3;
inline constexpr std::int32_t kOwners = 4;

/// What an operator's permission level is, from `op-permission-level`; set once at startup.
[[nodiscard]] std::int32_t operatorLevel() noexcept;
void setOperatorLevel(std::int32_t level) noexcept;

} // namespace permissionLevel

inline constexpr std::string_view kAdminPermission = "basalt.admin.broadcast";

class CommandSource {
public:
    CommandSource() = default;
    CommandSource(const CommandSource&) = default;
    CommandSource& operator=(const CommandSource&) = default;
    CommandSource(CommandSource&&) = default;
    CommandSource& operator=(CommandSource&&) = default;
    virtual ~CommandSource() = default;

    [[nodiscard]] virtual std::string name() const = 0;

    [[nodiscard]] virtual wire::text::Component displayName() const = 0;

    [[nodiscard]] virtual std::int32_t permissionLevel() const = 0;

    [[nodiscard]] virtual std::array<double, 3> position() const = 0;

    /// parity: `CommandSourceStack.getRotation`.
    [[nodiscard]] virtual std::pair<float, float> rotation() const { return {0.0F, 0.0F}; }

    [[nodiscard]] virtual player::PlayerSession* player() const = 0;

    virtual void sendSystemMessage(const wire::text::Component& message) const = 0;

    [[nodiscard]] virtual bool hasPermission(std::optional<std::string_view> node) const = 0;

    [[nodiscard]] virtual const ExecutionSource* asExecution() const { return nullptr; }

    /// `console`, `player`, `command_block` or `other`, for the plugin API to hand the right kind of sender.
    [[nodiscard]] virtual std::string_view kind() const { return "other"; }

    [[nodiscard]] virtual bool defersSelectors() const { return false; }

    /// parity: `CommandSourceStack.isSilent`.
    [[nodiscard]] virtual bool silent() const { return false; }

    /// parity: `CommandSourceStack.getSigningContext`.
    [[nodiscard]] virtual const chat::SignedArguments* signedArguments() const { return nullptr; }
};

class ConsoleCommandSource final : public CommandSource {
public:
    /// parity: `MinecraftServer.createCommandSourceStack`.
    using SpawnAt = std::function<std::array<double, 3>()>;

    explicit ConsoleCommandSource(wire::common::Logger logger = wire::common::Logger{"Console"},
                                  wire::text::LangTable lang = wire::text::LangTable{}, SpawnAt spawn = {});

    [[nodiscard]] std::string name() const override { return "Server"; }

    [[nodiscard]] std::string_view kind() const override { return "console"; }

    [[nodiscard]] wire::text::Component displayName() const override;

    [[nodiscard]] std::int32_t permissionLevel() const override { return permissionLevel::kOwners; }

    [[nodiscard]] std::array<double, 3> position() const override;

    [[nodiscard]] player::PlayerSession* player() const override { return nullptr; }

    void sendSystemMessage(const wire::text::Component& message) const override;

    [[nodiscard]] bool hasPermission(std::optional<std::string_view> node) const override;

private:
    wire::common::Logger logger_;
    wire::text::LangTable lang_;
    SpawnAt spawn_;
};

class PlayerCommandSource final : public CommandSource {
public:
    PlayerCommandSource(player::PlayerSession& player, const config::GroupsConfig& groups,
                        const chat::SignedArguments* signedArguments = nullptr);

    [[nodiscard]] std::string name() const override;

    [[nodiscard]] std::string_view kind() const override { return "player"; }

    [[nodiscard]] wire::text::Component displayName() const override;

    [[nodiscard]] std::int32_t permissionLevel() const override;

    [[nodiscard]] std::array<double, 3> position() const override;

    [[nodiscard]] player::PlayerSession* player() const override { return player_; }

    [[nodiscard]] std::pair<float, float> rotation() const override;

    void sendSystemMessage(const wire::text::Component& message) const override;

    [[nodiscard]] bool hasPermission(std::optional<std::string_view> node) const override;

    [[nodiscard]] const chat::SignedArguments* signedArguments() const override { return signedArguments_; }

private:
    player::PlayerSession* player_;
    const config::GroupsConfig* groups_;
    const chat::SignedArguments* signedArguments_;
};

/// parity: `BaseCommandBlock.createCommandSourceStack`.
class CommandBlockSource final : public CommandSource {
public:
    CommandBlockSource(std::array<double, 3> at, std::string name, bool speaks, wire::common::Logger logger,
                       wire::text::LangTable lang = wire::text::LangTable{});

    [[nodiscard]] std::string name() const override { return name_; }

    [[nodiscard]] std::string_view kind() const override { return "command_block"; }

    [[nodiscard]] wire::text::Component displayName() const override;

    [[nodiscard]] std::int32_t permissionLevel() const override { return permissionLevel::kGamemasters; }

    [[nodiscard]] std::array<double, 3> position() const override { return at_; }

    [[nodiscard]] player::PlayerSession* player() const override { return nullptr; }

    void sendSystemMessage(const wire::text::Component& message) const override;

    [[nodiscard]] bool hasPermission(std::optional<std::string_view> node) const override;

private:
    std::array<double, 3> at_;
    std::string name_;
    bool speaks_;
    wire::common::Logger logger_;
    wire::text::LangTable lang_;
};

} // namespace basalt::server::runtime::command
