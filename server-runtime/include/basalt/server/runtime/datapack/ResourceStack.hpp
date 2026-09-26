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

/// The selected packs as one immutable data tree, bottom pack first; any thread may read it.
///
/// parity: `MultiPackResourceManager`: a name comes from the highest pack that has it unless a pack
/// above filters it out. Tag files instead merge across every pack, as `TagLoader.load` does.
class ResourceStack {
public:
    struct Layer {
        std::shared_ptr<const PackContents> contents;
        std::vector<IdentifierPattern> filter;
    };

    /// One file as a pack supplied it.
    struct Source {
        std::string packId;
        std::vector<std::uint8_t> bytes;
    };

    explicit ResourceStack(std::vector<Layer> layers, wire::common::Logger logger = wire::common::Logger{"Datapacks"});

    /// The file under `data/<namespace>/<path>`, or nothing.
    [[nodiscard]] std::optional<std::vector<std::uint8_t>> read(std::string_view name) const;

    /// The same as text; a tag file comes back already merged across packs.
    [[nodiscard]] std::optional<std::string> readText(std::string_view name) const;

    /// Every visible name under a prefix, sorted.
    [[nodiscard]] std::vector<std::string> entriesUnder(std::string_view prefix) const;

    /// Every copy of a file, bottom pack first, minus those a pack above filtered away.
    /// parity: `getResourceStack`.
    [[nodiscard]] std::vector<Source> stackOf(std::string_view name) const;

    /// `{"values": [...]}` with every pack's entries, a `replace` clearing what came before.
    /// A file that does not parse is logged and left out. Nothing when no pack has the tag.
    [[nodiscard]] std::optional<std::string> mergedTag(std::string_view name) const;

    /// Every id under `data/<namespace>/<directory>/` with that extension.
    [[nodiscard]] std::vector<wire::common::Identifier> dataIds(std::string_view directory,
                                                                std::string_view extension) const;

    /// Which pack supplies a name, or nothing.
    [[nodiscard]] std::optional<std::string> packOf(std::string_view name) const;

    [[nodiscard]] std::vector<std::string> packIds() const;

private:
    struct CompiledPattern {
        std::optional<std::regex> nameSpace;
        std::optional<std::regex> path;
    };

    /// parity: a namespace is filtered when any pattern's namespace matches it, and then a path
    /// when any pattern's path matches, the two halves not taken from the same pattern.
    [[nodiscard]] bool filters(std::size_t layer, std::string_view name) const;

    std::vector<Layer> layers_;
    std::vector<std::vector<CompiledPattern>> compiled_;
    /// Every visible name, and the index of the layer that supplies it.
    std::map<std::string, std::size_t, std::less<>> visible_;
    wire::common::Logger logger_;
};

} // namespace basalt::server::runtime::datapack
