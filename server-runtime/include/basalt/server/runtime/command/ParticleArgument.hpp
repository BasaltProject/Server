#pragma once

#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/command/CommandTree.hpp>
#include <basalt/wire/protocol/play/ParticleOptions.hpp>

#include <memory>
#include <optional>

namespace basalt::server::runtime::command {

struct ParsedParticle {
    const registry::ParticleType* type = nullptr;
    std::optional<wire::protocol::play::ParticleOptions> options;
};

/// parity: `ParticleArgument.readParticle`.
[[nodiscard]] std::shared_ptr<const ArgumentType> particleArgument();

} // namespace basalt::server::runtime::command
