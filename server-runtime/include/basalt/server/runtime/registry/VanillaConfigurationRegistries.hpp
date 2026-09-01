#pragma once

#include <basalt/wire/protocol/configuration/ConfigurationPackets.hpp>

#include <vector>

namespace basalt::server::runtime::registry {

/// The static vanilla registry packets sent during client configuration, in the order they go out.
///
/// Order is not cosmetic: each entry's index within its packet is the numeric holder id the client
/// resolves later, so a registry sent short or out of order is a connection that drops when a
/// packet references an id the client does not have.
namespace vanillaConfigurationRegistries {

[[nodiscard]] std::vector<wire::protocol::configuration::RegistryDataPacket> packets();

} // namespace vanillaConfigurationRegistries

} // namespace basalt::server::runtime::registry
