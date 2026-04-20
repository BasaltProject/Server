#pragma once

#include <basalt/server/runtime/datapack/PackFiles.hpp>
#include <basalt/server/runtime/datapack/PackMetadata.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/common/logging/Logger.hpp>

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <regex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::server::runtime::datapack {

/// parity: `MultiPackResourceManager`.
class ResourceStack {
public:
    struct Layer {
        std::shared_ptr<const PackContents> contents;
        std::vector<IdentifierPattern> filter;
    };

    struct Source {
        std::string packId;
        std::vector<std::uint8_t> bytes;
    };

    explicit ResourceStack(std::vector<Layer> layers, wire::common::Logger logger = wire::common::Logger{"Datapacks"});

    [[nodiscard]] std::optional<std::vector<std::uint8_t>> read(std::string_view name) const;

    [[nodiscard]] std::optional<std::string> readText(std::string_view name) const;

    [[nodiscard]] std::vector<std::string> entriesUnder(std::string_view prefix) const;

    /// parity: `getResourceStack`.
    [[nodiscard]] std::vector<Source> stackOf(std::string_view name) const;

    [[nodiscard]] std::optional<std::string> mergedTag(std::string_view name) const;

    [[nodiscard]] std::vector<wire::common::Identifier> dataIds(std::string_view directory,
                                                                std::string_view extension) const;

    [[nodiscard]] std::optional<std::string> packOf(std::string_view name) const;

    [[nodiscard]] std::vector<std::string> packIds() const;

private:
    struct CompiledPattern {
        std::optional<std::regex> nameSpace;
        std::optional<std::regex> path;
    };

    [[nodiscard]] bool filters(std::size_t layer, std::string_view name) const;

    std::vector<Layer> layers_;
    std::vector<std::vector<CompiledPattern>> compiled_;
    std::map<std::string, std::size_t, std::less<>> visible_;
    wire::common::Logger logger_;
};

} // namespace basalt::server::runtime::datapack
