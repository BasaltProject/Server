#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>
#include <basalt/server/runtime/world/PlayerProgress.hpp>
#include <basalt/server/runtime/world/RealRecipes.hpp>

namespace basalt::server::runtime::command::commands {

/// parity: `AdvancementCommands`, all five selection modes.
///
/// Granting means marking every criterion of the advancement done, which is what vanilla's
/// `AdvancementProgress.grantProgress` ends up doing.
class AdvancementCommand final : public Command {
public:
    AdvancementCommand(const world::RealAdvancements& advancements, world::PlayerProgress& progress);

    void build(const CommandNodeBuilder& root) const override;

private:
    const world::RealAdvancements* advancements_;
    world::PlayerProgress* progress_;
};

/// parity: `RecipeCommand`.
///
/// Both halves are real: the server remembers what a player knows (which is what
/// `limited_crafting` consults before letting them craft), and the client's own recipe book is
/// told, so the recipe appears in it. A recipe whose type this build cannot draw is still
/// remembered; it simply is not sent to the book, because drawing it wrong would be worse than not
/// drawing it.
class RecipeCommand final : public Command {
public:
    RecipeCommand(world::PlayerProgress& progress, const world::RealRecipes& recipes);

    void build(const CommandNodeBuilder& root) const override;

private:
    world::PlayerProgress* progress_;
    const world::RealRecipes* recipes_;
};

} // namespace basalt::server::runtime::command::commands
