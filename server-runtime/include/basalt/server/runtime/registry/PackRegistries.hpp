#pragma once

#include <basalt/wire/common/logging/Logger.hpp>
#include <basalt/wire/protocol/configuration/ConfigurationPackets.hpp>

#include <string>
#include <vector>

namespace basalt::server::runtime::datapack {
class ResourceStack;
}

namespace basalt::server::runtime::registry {

/// parity: `RegistrySynchronization.packRegistries`.
void applyPackRegistries(std::vector<wire::protocol::configuration::RegistryDataPacket>& packets,
                         const datapack::ResourceStack& resources, const std::vector<std::string>& builtInPacks,
                         const wire::common::Logger& logger = wire::common::Logger{"Registries"});

void installRegistries(std::vector<wire::protocol::configuration::RegistryDataPacket> packets);

[[nodiscard]] std::vector<wire::protocol::configuration::RegistryDataPacket> installedRegistries();

} // namespace basalt::server::runtime::registry
