#pragma once

#include <basalt/server/runtime/datapack/ResourceStack.hpp>
#include <basalt/wire/common/logging/Logger.hpp>

#include <string>
#include <vector>

namespace basalt::server::runtime::datapack {

/// One registry element a pack wrote that cannot be read.
struct ElementError {
    std::string registry;
    std::string element;
    std::string message;
};

/// The elements of the reloadable registries (and the world ones read at startup) that the packs
/// outside `builtInPacks` got wrong: not JSON, or not an object. parity: `RegistryDataLoader.load`,
/// where one such element fails the whole load. Only the file's own shape is judged here, because
/// a type this build cannot model yet is this build's gap and not the pack's error.
/// `worldRegistries` adds the registries only a start reads, which `/reload` leaves alone.
[[nodiscard]] std::vector<ElementError>
invalidElements(const ResourceStack& resources, const std::vector<std::string>& builtInPacks, bool worldRegistries);

/// The packs a client can hold a copy of: every one but the world's own.
[[nodiscard]] std::vector<std::string> builtInPackIds(const class PackRepository& repository);

/// parity: `RegistryDataLoader.printFullDetailsToLog`, grouped by registry then element.
void logElementErrors(const std::vector<ElementError>& errors, const wire::common::Logger& logger);

} // namespace basalt::server::runtime::datapack
