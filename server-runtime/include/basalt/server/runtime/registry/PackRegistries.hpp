#pragma once

#include <basalt/wire/common/logging/Logger.hpp>
#include <basalt/wire/protocol/configuration/ConfigurationPackets.hpp>

#include <string>
#include <vector>

namespace basalt::server::runtime::datapack {
class ResourceStack;
}

namespace basalt::server::runtime::registry {

/// Lays the selected data packs over the synchronised registries this build sends.
///
/// parity: `RegistrySynchronization.packRegistries`: an entry a pack the client does not know
/// writes goes out with that pack's data. Divergence: a pack's new entries go after the built-in
/// ones instead of in id order, because this build numbers the built-in ones by their fixed order.
void applyPackRegistries(std::vector<wire::protocol::configuration::RegistryDataPacket>& packets,
                         const datapack::ResourceStack& resources, const std::vector<std::string>& builtInPacks,
                         const wire::common::Logger& logger = wire::common::Logger{"Registries"});

} // namespace basalt::server::runtime::registry
