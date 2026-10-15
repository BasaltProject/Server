#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>
#include <basalt/server/runtime/world/PlayerProgress.hpp>
#include <basalt/server/runtime/world/RealRecipes.hpp>

namespace basalt::server::runtime::command::commands {

/// parity: `AdvancementCommands`, all five selection modes.
class AdvancementCommand final : public Command {
public:
    AdvancementCommand(const world::RealAdvancements& advancements, world::PlayerProgress& progress);

    void build(const CommandNodeBuilder& root) const override;

private:
    const world::RealAdvancements* advancements_;
    world::PlayerProgress* progress_;
};

/// parity: `RecipeCommand`.
class RecipeCommand final : public Command {
public:
    RecipeCommand(world::PlayerProgress& progress, const world::RealRecipes& recipes);

    void build(const CommandNodeBuilder& root) const override;

private:
    world::PlayerProgress* progress_;
    const world::RealRecipes* recipes_;
};

} // namespace basalt::server::runtime::command::commands
