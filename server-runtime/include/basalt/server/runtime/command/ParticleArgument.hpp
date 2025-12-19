#pragma once

#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/command/CommandTree.hpp>
#include <basalt/wire/protocol/play/ParticleOptions.hpp>

#include <memory>
#include <optional>

namespace basalt::server::runtime::command {

/// A particle as `/particle` names it: the type, and the options it carries.
struct ParsedParticle {
    const registry::ParticleType* type = nullptr;
    std::optional<wire::protocol::play::ParticleOptions> options;
};

/// `minecraft:particle`: a type id and, for the types that take one, an SNBT block of options.
///
/// parity: `ParticleArgument.readParticle`, which reads the id and then hands whatever compound
/// follows to that type's own codec. The field names are those codecs' own (`block_state`, `color`,
/// `from_color`, `water_blocks`), because they are what a command writes and what a datapack
/// already contains. The value is a `ParsedParticle`.
[[nodiscard]] std::shared_ptr<const ArgumentType> particleArgument();

} // namespace basalt::server::runtime::command
