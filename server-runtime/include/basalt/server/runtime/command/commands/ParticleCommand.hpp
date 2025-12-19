#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>

namespace basalt::server::runtime::command::commands {

/// parity: `ParticleCommand`.
///
/// The options a type carries are parsed and written by the particle argument: they go straight
/// after the type id with no length of their own, so a missing or wrongly shaped block
/// desynchronises the client on the very next packet rather than drawing the wrong particle.
class ParticleCommand final : public Command {
public:
    ParticleCommand();

    void build(const CommandNodeBuilder& root) const override;
};

} // namespace basalt::server::runtime::command::commands
