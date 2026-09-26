#include <basalt/server/runtime/world/StructureTemplates.hpp>
#include <basalt/wire/io/Compression.hpp>
#include <basalt/wire/nbt/NbtIO.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <fstream>
#include <iterator>

namespace basalt::server::runtime::world {
namespace {

using wire::common::Identifier;
using Template = server::world::structure::StructureTemplate;

/// The structure file of an id, which keeps its own namespace: a datapack ships its own directory.
[[nodiscard]] std::string entryFor(const Identifier& id) {
    return "data/" + id.namespaceName() + "/structure/" + id.path() + ".nbt";
}

} // namespace

void StructureTemplates::reload(Jar jar) {
    const std::scoped_lock lock{mutex_};
    retired_ = std::move(cache_);
    cache_.clear();
    listings_.clear();
    jar_ = std::move(jar);
}

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
    // parity: the generated directory is the first source a template is looked for in.
    std::optional<std::vector<std::uint8_t>> generated;
    if (generatedRoot_.has_value()) {
        std::ifstream file{*generatedRoot_ / id.namespaceName() / "structure" / (id.path() + ".nbt"), std::ios::binary};
        if (file) {
            generated = std::vector<std::uint8_t>{std::istreambuf_iterator<char>{file}, {}};
        }
    }
    if (!jar_.readEntry && !generated.has_value()) {
        refusal = "no jar";
    } else if (const std::optional<std::vector<std::uint8_t>> bytes =
                   generated.has_value() ? generated : jar_.readEntry(entryFor(id));
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

void StructureTemplates::setGeneratedRoot(std::filesystem::path root) { generatedRoot_ = std::move(root); }

bool StructureTemplates::store(const Identifier& id, const wire::nbt::NbtCompound& structure, bool toDisk) {
    Template built;
    built.load(structure);
    {
        const std::scoped_lock lock{mutex_};
        cache_.insert_or_assign(id, std::move(built));
    }
    if (!toDisk) {
        return true;
    }
    if (!generatedRoot_.has_value()) {
        return false;
    }
    const std::filesystem::path file = *generatedRoot_ / id.namespaceName() / "structure" / (id.path() + ".nbt");
    std::error_code failed;
    std::filesystem::create_directories(file.parent_path(), failed);
    const std::expected<std::vector<std::uint8_t>, wire::io::CompressionError> packed =
        wire::io::gzip::compress(wire::nbt::nbtIO::write(wire::nbt::NbtTag::ofCompound(structure)));
    if (failed || !packed.has_value()) {
        logger_.warn([&] { return "Failed to save structure file " + id.toString(); });
        return false;
    }
    std::ofstream out{file, std::ios::binary | std::ios::trunc};
    out.write(reinterpret_cast<const char*>(packed->data()), static_cast<std::streamsize>(packed->size()));
    return static_cast<bool>(out);
}

void StructureTemplates::forget(const Identifier& id) {
    const std::scoped_lock lock{mutex_};
    cache_.erase(id);
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
