#pragma once

#include <basalt/wire/protocol/configuration/ConfigurationPackets.hpp>

#include <vector>

namespace basalt::server::runtime::registry {

namespace vanillaConfigurationRegistries {

[[nodiscard]] std::vector<wire::protocol::configuration::RegistryDataPacket> packets();

} // namespace vanillaConfigurationRegistries

} // namespace basalt::server::runtime::registry
