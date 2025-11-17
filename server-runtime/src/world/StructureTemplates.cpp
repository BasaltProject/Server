#include <basalt/server/runtime/world/StructureTemplates.hpp>
#include <basalt/wire/io/Compression.hpp>
#include <basalt/wire/nbt/NbtIO.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

namespace basalt::server::runtime::world {
namespace {

using wire::common::Identifier;
using Template = server::world::structure::StructureTemplate;

/// The structure file of an id, which keeps its own namespace: a datapack ships its own directory.
[[nodiscard]] std::string entryFor(const Identifier& id) {
    return "data/" + id.namespaceName() + "/structure/" + id.path() + ".nbt";
}

} // namespace

const Template* StructureTemplates::find(const Identifier& id) {
    {
        const std::scoped_lock lock{mutex_};
        const auto found = cache_.find(id);
        if (found != cache_.end()) {
            return found->second.has_value() ? &*found->second : nullptr;
        }
    }
    std::optional<Template> loaded;
    std::string refusal;
    if (!jar_.readEntry) {
        refusal = "no jar";
    } else if (const std::optional<std::vector<std::uint8_t>> bytes = jar_.readEntry(entryFor(id));
               !bytes.has_value()) {
        refusal = "no such structure file";
    } else if (const std::expected<std::vector<std::uint8_t>, wire::io::CompressionError> plain =
                   wire::io::gzip::decompress(*bytes);
               !plain.has_value()) {
        refusal = "not gzip";
    } else if (const std::expected<wire::nbt::NamedTag, wire::nbt::NbtError> read = wire::nbt::nbtIO::read(*plain);
               !read.has_value()) {
        refusal = "not NBT";
    } else if (const wire::nbt::NbtCompound* root = read->tag.get<wire::nbt::NbtCompound>(); root == nullptr) {
        refusal = "not a compound";
    } else {
        Template built;
        built.load(*root);
        loaded = std::move(built);
    }
    if (!loaded.has_value()) {
        logger_.debug([&] { return "No structure '" + id.toString() + "': " + refusal; });
    }

    const std::scoped_lock lock{mutex_};
    // Another thread may have answered the same name first; its entry is the one that stands.
    const auto [entry, inserted] = cache_.emplace(id, std::move(loaded));
    return entry->second.has_value() ? &*entry->second : nullptr;
}

const std::vector<Identifier>& StructureTemplates::templateIds() { return idsUnder("structure", ".nbt"); }

const std::vector<Identifier>& StructureTemplates::poolIds() { return idsUnder("worldgen/template_pool", ".json"); }

const std::vector<Identifier>& StructureTemplates::idsUnder(const std::string& directory, std::string_view extension) {
    {
        const std::scoped_lock lock{mutex_};
        const auto found = listings_.find(directory);
        if (found != listings_.end()) {
            return found->second;
        }
    }
    // Listing the jar is a directory walk, so each answer is kept rather than walked again.
    std::vector<Identifier> ids = jar_.dataIds ? jar_.dataIds(directory, extension) : std::vector<Identifier>{};

    const std::scoped_lock lock{mutex_};
    return listings_.emplace(directory, std::move(ids)).first->second;
}

} // namespace basalt::server::runtime::world
