#pragma once

#include <basalt/server/runtime/command/CommandEnvironment.hpp>
#include <basalt/server/runtime/command/CommandSource.hpp>
#include <basalt/server/runtime/command/CommandTarget.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <utility>

namespace basalt::server::runtime::command {

/// A command source with pieces swapped out, which is all `/execute` does.
///
/// parity: `CommandSourceStack.withEntity`/`withPosition`/`withLevel`. Everything else is delegated
/// to whoever started the chain, so feedback still reaches them and permissions are still theirs:
/// `execute as <player>` runs *as* that player but not *with their permissions*, exactly as
/// vanilla.
class ExecutionSource final : public CommandSource {
public:
    /// What `/execute store` hangs here: called with the result of whatever the chain ends up
    /// running.
    using OnResult = std::function<void(bool success, std::int32_t result)>;

    /// What this link changes about the source it wraps. Everything left unset is inherited.
    struct Changes {
        std::optional<player::PlayerSession*> player;
        std::optional<std::array<double, 3>> position;
        /// The world this link acts in, or unset to use whatever the base source would.
        std::optional<world::LoadedWorld*> world;
        std::optional<std::pair<float, float>> rotation;
        /// parity: `anchored`, which decides whether a position means the feet or the eyes.
        std::optional<bool> anchorEyes;
        /// Who `@s` is, which `/execute as` sets and which can be something other than a player.
        std::optional<CommandTarget> entity;
        OnResult onResult;
    };

    ExecutionSource(const CommandSource& base, Changes changes = {});

    [[nodiscard]] std::string name() const override;

    [[nodiscard]] wire::text::Component displayName() const override;

    [[nodiscard]] std::int32_t permissionLevel() const override;

    [[nodiscard]] std::array<double, 3> position() const override { return position_; }

    [[nodiscard]] player::PlayerSession* player() const override { return player_; }

    void sendSystemMessage(const wire::text::Component& message) const override;

    [[nodiscard]] bool hasPermission(std::optional<std::string_view> node) const override;

    [[nodiscard]] const ExecutionSource* asExecution() const override { return this; }

    /// parity: a modified `CommandSourceStack` keeps the signing context it came from.
    [[nodiscard]] const chat::SignedArguments* signedArguments() const override { return base_->signedArguments(); }

    [[nodiscard]] world::LoadedWorld* world() const noexcept { return world_; }

    [[nodiscard]] std::pair<float, float> rotation() const override { return rotation_; }

    [[nodiscard]] bool anchorEyes() const noexcept { return anchorEyes_; }

    [[nodiscard]] const std::optional<CommandTarget>& entity() const noexcept { return entity_; }

    /// The source that started the chain, which is who feedback belongs to.
    [[nodiscard]] const CommandSource& root() const;

    /// Reports a result to every `store` in the chain, innermost first.
    void report(std::int32_t result) const;

private:
    const CommandSource* base_;
    player::PlayerSession* player_ = nullptr;
    std::array<double, 3> position_{0.0, 0.0, 0.0};
    world::LoadedWorld* world_ = nullptr;
    std::pair<float, float> rotation_{0.0F, 0.0F};
    bool anchorEyes_ = false;
    std::optional<CommandTarget> entity_;
    OnResult onResult_;
};

/// The world a source acts in: what `/execute in` chose, else the runner's own.
[[nodiscard]] world::LoadedWorld* worldIn(const CommandSource& source, const CommandEnvironment& environment);

} // namespace basalt::server::runtime::command
