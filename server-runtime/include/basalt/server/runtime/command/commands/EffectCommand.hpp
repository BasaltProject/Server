#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>

namespace basalt::server::runtime::command::commands {

/// parity: `EffectCommand`.
///
/// Only players carry effects here: nothing else in this build has the health or the hunger the
/// effects act on, so a selector that matches something else is refused by name.
class EffectCommand final : public Command {
public:
    EffectCommand();

    void build(const CommandNodeBuilder& root) const override;
};

/// `/world`: which worlds exist, and moving between them.
///
/// No vanilla counterpart, because vanilla has no worlds to move between: it has three dimensions
/// of one world, reached by building a portal. `/execute in <dimension>` is the closest thing and
/// it is a different idea. So this is Basalt's, like `import-players`.
///
/// The bare form keeps the traveller's coordinates, which is what vanilla's dimension change
/// (`/execute in <dim> run tp @s ~ ~ ~`) does; `spawn` lands on the world's spawn instead.
///
/// Travel itself is not reimplemented here: it is the same `switchWorld` that `/tp ... <world>`
/// uses, which is what keeps one way of changing world rather than two that can drift.
class WorldCommand final : public Command {
public:
    WorldCommand();

    void build(const CommandNodeBuilder& root) const override;
};

} // namespace basalt::server::runtime::command::commands
