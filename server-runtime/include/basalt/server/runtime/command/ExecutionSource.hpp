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
#include <vector>

namespace basalt::server::runtime::command {

/// parity: `CommandSourceStack.withEntity`/`withPosition`/`withLevel`.
class ExecutionSource final : public CommandSource {
public:
    using OnResult = std::function<void(bool success, std::int32_t result)>;

    struct Changes {
        std::optional<player::PlayerSession*> player;
        std::optional<std::array<double, 3>> position;
        std::optional<world::LoadedWorld*> world;
        std::optional<std::pair<float, float>> rotation;
        /// parity: `anchored`, which decides whether a position means the feet or the eyes.
        std::optional<bool> anchorEyes;
        std::optional<CommandTarget> entity;
        OnResult onResult;
        /// parity: `withPermission`, what a function runs its lines with.
        std::optional<std::int32_t> permission;
        /// parity: `withSuppressedOutput`.
        bool silent;
        bool trusted;
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

    [[nodiscard]] std::string_view kind() const override;

    [[nodiscard]] bool silent() const override { return silent_; }

    /// parity: a modified `CommandSourceStack` keeps the signing context it came from.
    [[nodiscard]] const chat::SignedArguments* signedArguments() const override { return base_->signedArguments(); }

    [[nodiscard]] world::LoadedWorld* world() const noexcept { return world_; }

    [[nodiscard]] std::pair<float, float> rotation() const override { return rotation_; }

    [[nodiscard]] bool anchorEyes() const noexcept { return anchorEyes_; }

    [[nodiscard]] const std::optional<CommandTarget>& entity() const noexcept { return entity_; }

    [[nodiscard]] const CommandSource& root() const;

    void report(std::int32_t result) const;

    [[nodiscard]] bool observesResult() const;

    [[nodiscard]] std::vector<OnResult> resultCallbacks() const;

private:
    const CommandSource* base_;
    player::PlayerSession* player_ = nullptr;
    std::array<double, 3> position_{0.0, 0.0, 0.0};
    world::LoadedWorld* world_ = nullptr;
    std::pair<float, float> rotation_{0.0F, 0.0F};
    bool anchorEyes_ = false;
    std::optional<CommandTarget> entity_;
    OnResult onResult_;
    std::optional<std::int32_t> permission_;
    bool silent_ = false;
    bool trusted_ = false;
};

[[nodiscard]] world::LoadedWorld* worldIn(const CommandSource& source, const CommandEnvironment& environment);

} // namespace basalt::server::runtime::command
