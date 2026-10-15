#pragma once

#include <basalt/server/runtime/datapack/ResourceStack.hpp>
#include <basalt/wire/common/logging/Logger.hpp>

#include <string>
#include <vector>

namespace basalt::server::runtime::datapack {

struct ElementError {
    std::string registry;
    std::string element;
    std::string message;
};

/// parity: `RegistryDataLoader.load`, where one such element fails the whole load.
[[nodiscard]] std::vector<ElementError>
invalidElements(const ResourceStack& resources, const std::vector<std::string>& builtInPacks, bool worldRegistries);

[[nodiscard]] std::vector<std::string> builtInPackIds(const class PackRepository& repository);

/// parity: `RegistryDataLoader.printFullDetailsToLog`, grouped by registry then element.
void logElementErrors(const std::vector<ElementError>& errors, const wire::common::Logger& logger);

} // namespace basalt::server::runtime::datapack
