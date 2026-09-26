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
/// writes goes out with that pack's data, and a new one joins the registry.
void applyPackRegistries(std::vector<wire::protocol::configuration::RegistryDataPacket>& packets,
                         const datapack::ResourceStack& resources, const std::vector<std::string>& builtInPacks,
                         const wire::common::Logger& logger = wire::common::Logger{"Registries"});

/// Puts the data-driven registries in id order, as `RegistryDataLoader` registers them, and makes
/// them what every client is sent and what the server numbers entries by. Done once, at startup:
/// these are world registries, which `/reload` leaves alone.
void installRegistries(std::vector<wire::protocol::configuration::RegistryDataPacket> packets);

/// What a client is sent at configuration: the installed registries, or the built-in ones.
[[nodiscard]] std::vector<wire::protocol::configuration::RegistryDataPacket> installedRegistries();

} // namespace basalt::server::runtime::registry
