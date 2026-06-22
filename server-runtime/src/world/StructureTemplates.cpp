#include <basalt/server/runtime/world/StructureTemplates.hpp>
#include <basalt/wire/io/Compression.hpp>
#include <basalt/wire/nbt/NbtIO.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <algorithm>
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
        kept_.insert_or_assign(id, structure);
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
    kept_.erase(id);
}

std::filesystem::path StructureTemplates::generatedFile(const Identifier& id) const {
    return generatedRoot_.has_value() ? *generatedRoot_ / id.namespaceName() / "structure" / (id.path() + ".nbt")
                                      : std::filesystem::path{};
}

std::optional<wire::nbt::NbtCompound> StructureTemplates::read(const Identifier& id) {
    {
        const std::scoped_lock lock{mutex_};
        if (const auto held = kept_.find(id); held != kept_.end()) {
            return held->second;
        }
    }
    std::optional<std::vector<std::uint8_t>> bytes;
    if (const std::filesystem::path generated = generatedFile(id); !generated.empty()) {
        std::ifstream file{generated, std::ios::binary};
        if (file) {
            bytes = std::vector<std::uint8_t>{std::istreambuf_iterator<char>{file}, {}};
        }
    }
    if (!bytes.has_value() && jar_.readEntry) {
        bytes = jar_.readEntry(entryFor(id));
    }
    if (!bytes.has_value()) {
        return std::nullopt;
    }
    const std::expected<std::vector<std::uint8_t>, wire::io::CompressionError> plain =
        wire::io::gzip::decompress(*bytes);
    if (!plain.has_value()) {
        return std::nullopt;
    }
    const std::expected<wire::nbt::NamedTag, wire::nbt::NbtError> parsed = wire::nbt::nbtIO::read(*plain);
    const wire::nbt::NbtCompound* root = parsed.has_value() ? parsed->tag.get<wire::nbt::NbtCompound>() : nullptr;
    return root == nullptr ? std::nullopt : std::optional{*root};
}

bool StructureTemplates::erase(const Identifier& id, bool fromDisk) {
    forget(id);
    if (!fromDisk) {
        return true;
    }
    const std::filesystem::path file = generatedFile(id);
    std::error_code failed;
    return !file.empty() && std::filesystem::remove(file, failed) && !failed;
}

std::vector<Identifier> StructureTemplates::allIds() {
    std::vector<Identifier> ids = templateIds();
    if (generatedRoot_.has_value()) {
        std::error_code failed;
        for (const auto& space : std::filesystem::directory_iterator(*generatedRoot_, failed)) {
            const std::filesystem::path folder = space.path() / "structure";
            for (std::filesystem::recursive_directory_iterator walk{folder, failed};
                 !failed && walk != std::filesystem::recursive_directory_iterator{}; walk.increment(failed)) {
                if (walk->path().extension() != ".nbt") {
                    continue;
                }
                std::string name = std::filesystem::relative(walk->path(), folder).replace_extension().generic_string();
                if (const auto parsed = Identifier::parse(space.path().filename().string() + ":" + name);
                    parsed.has_value()) {
                    ids.push_back(*parsed);
                }
            }
        }
    }
    const std::scoped_lock lock{mutex_};
    for (const auto& [id, structure] : kept_) {
        ids.push_back(id);
    }
    std::ranges::sort(ids);
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    return ids;
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
