#pragma once

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

namespace basalt::server::runtime::command {

class ExecutionSource;

/// Vanilla's own permission ladder (`Commands.LEVEL_*`), recorded per command as the parity fact it
/// is: it is what the real server demands for each of them, and what a client is eventually told
/// about itself.
///
/// It is **not** what authorises anything here. Authorisation is by named permission, because a
/// number in a configuration file tells an operator nothing and cannot be referred to by
/// `commands.toml` or by a plugin.
namespace permissionLevel {

inline constexpr std::int32_t kAll = 0;
inline constexpr std::int32_t kModerators = 1;
inline constexpr std::int32_t kGamemasters = 2;
inline constexpr std::int32_t kAdmins = 3;
inline constexpr std::int32_t kOwners = 4;

} // namespace permissionLevel

/// Seeing other people's successful commands, vanilla's `chat.type.admin` broadcast.
///
/// A permission rather than "level 2 or above", so that who receives them is decided in the same
/// file and the same way as everything else. `*` covers it, so an operator gets them by default.
inline constexpr std::string_view kAdminPermission = "basalt.admin.broadcast";

/// Whoever ran a command: a player, or the operator at the console.
///
/// Deliberately not a handle on "the server": commands reach the world and the player list through
/// `CommandEnvironment`, which is passed in, so nothing here needs a global. That is the same
/// reason the position lives on the source, the way vanilla's `CommandSourceStack` carries one:
/// relative coordinates (`~ ~ ~`) resolve against the runner, whoever that is.
///
/// Virtual because a source is asked about once per command, not once per block: this is the one
/// place in the runtime where a small hierarchy costs nothing and says exactly what it means.
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

    /// Where `~` resolves from. The console stands at the world's shared spawn, as vanilla's does.
    [[nodiscard]] virtual std::array<double, 3> position() const = 0;

    /// The player who ran this, or null for the console. Commands that need one say so themselves.
    [[nodiscard]] virtual player::PlayerSession* player() const = 0;

    virtual void sendSystemMessage(const wire::text::Component& message) const = 0;

    /// Whether this source has been granted a node. A node nobody granted is denied.
    [[nodiscard]] virtual bool hasPermission(std::optional<std::string_view> node) const = 0;

    /// This source as an `/execute` link, or null for one that is not.
    ///
    /// Asked rather than cast: there is no RTTI here, so a source that wants to be recognised says
    /// so itself. `/execute` is the only thing that asks.
    [[nodiscard]] virtual const ExecutionSource* asExecution() const { return nullptr; }
};

/// The console. Permission `kOwners`, like vanilla's own `MinecraftServer` source: whoever can type
/// here can already stop the process.
class ConsoleCommandSource final : public CommandSource {
public:
    /// parity: `MinecraftServer.createCommandSourceStack` stands the console at the overworld's
    /// shared spawn, not the origin, so `/locate` and relative coordinates answer from there. Asked
    /// lazily: the world is not loaded when the server builds this.
    using SpawnAt = std::function<std::array<double, 3>()>;

    explicit ConsoleCommandSource(wire::common::Logger logger = wire::common::Logger{"Console"},
                                  wire::text::LangTable lang = wire::text::LangTable{}, SpawnAt spawn = {});

    [[nodiscard]] std::string name() const override { return "Server"; }

    [[nodiscard]] wire::text::Component displayName() const override;

    [[nodiscard]] std::int32_t permissionLevel() const override { return permissionLevel::kOwners; }

    [[nodiscard]] std::array<double, 3> position() const override;

    [[nodiscard]] player::PlayerSession* player() const override { return nullptr; }

    void sendSystemMessage(const wire::text::Component& message) const override;

    /// Everything, unconditionally: whoever can type here can already stop the process, and making
    /// the console depend on a group would let a typo in `groups.toml` lock the operator out.
    [[nodiscard]] bool hasPermission(std::optional<std::string_view> node) const override;

private:
    wire::common::Logger logger_;
    wire::text::LangTable lang_;
    SpawnAt spawn_;
};

/// A player running a command in chat. Their feedback goes back to them as a system message, as
/// vanilla's does.
class PlayerCommandSource final : public CommandSource {
public:
    PlayerCommandSource(player::PlayerSession& player, const config::GroupsConfig& groups);

    [[nodiscard]] std::string name() const override;

    [[nodiscard]] wire::text::Component displayName() const override;

    /// Only meaningful for the vanilla-facing bridge; nothing here authorises on it.
    [[nodiscard]] std::int32_t permissionLevel() const override;

    [[nodiscard]] std::array<double, 3> position() const override;

    [[nodiscard]] player::PlayerSession* player() const override { return player_; }

    void sendSystemMessage(const wire::text::Component& message) const override;

    [[nodiscard]] bool hasPermission(std::optional<std::string_view> node) const override;

private:
    player::PlayerSession* player_;
    const config::GroupsConfig* groups_;
};

} // namespace basalt::server::runtime::command
