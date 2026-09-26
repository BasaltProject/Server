#include <basalt/server/runtime/datapack/PackFiles.hpp>
#include <basalt/wire/io/FileIO.hpp>
#include <basalt/wire/io/ZipReader.hpp>

#include <algorithm>
#include <utility>

namespace basalt::server::runtime::datapack {
namespace {

constexpr std::string_view kDataRoot = "data/";

void walk(const std::string& root, const std::string& relative, std::vector<std::string>& out) {
    const std::string here = relative.empty() ? root : root + "/" + relative;
    for (const std::string& entry : wire::io::fileIO::list(here)) {
        const std::string child = relative.empty() ? entry : relative + "/" + entry;
        if (wire::io::fileIO::isDirectory(root + "/" + child)) {
            walk(root, child, out);
        } else {
            out.push_back(child);
        }
    }
}

[[nodiscard]] bool isNamespaceChar(char c) {
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-';
}

} // namespace

bool validNamespace(std::string_view name) { return !name.empty() && std::ranges::all_of(name, isNamespaceChar); }

bool validPath(std::string_view path) {
    return !path.empty() && std::ranges::all_of(path, [](char c) { return isNamespaceChar(c) || c == '/'; });
}

PackFiles directoryFiles(std::string root) {
    return PackFiles{
        .list =
            [root] {
                std::vector<std::string> out;
                walk(root, "", out);
                return out;
            },
        .read = [root](std::string_view path) { return wire::io::fileIO::readBytes(root + "/" + std::string{path}); },
    };
}

std::optional<PackFiles> zipFiles(const std::string& path) {
    std::optional<std::vector<std::uint8_t>> bytes = wire::io::fileIO::readBytes(path);
    if (!bytes.has_value()) {
        return std::nullopt;
    }
    std::expected<wire::io::ZipReader, wire::io::ZipError> opened = wire::io::ZipReader::open(std::move(*bytes));
    if (!opened.has_value()) {
        return std::nullopt;
    }
    auto archive = std::make_shared<const wire::io::ZipReader>(std::move(*opened));
    return PackFiles{
        .list =
            [archive] {
                std::vector<std::string> out;
                for (const std::string& name : archive->entryNames()) {
                    if (!name.ends_with('/')) {
                        out.push_back(name);
                    }
                }
                return out;
            },
        .read = [archive](std::string_view name) -> std::optional<std::vector<std::uint8_t>> {
            std::expected<std::vector<std::uint8_t>, wire::io::ZipError> read = archive->read(name);
            return read.has_value() ? std::optional{std::move(*read)} : std::nullopt;
        },
    };
}

PackFiles subtreeFiles(const PackFiles& whole, std::string prefix, std::vector<std::string> excluded) {
    return PackFiles{
        .list =
            [whole, prefix, excluded] {
                std::vector<std::string> out;
                if (!whole.list) {
                    return out;
                }
                for (const std::string& name : whole.list()) {
                    if (!name.starts_with(prefix) || name.size() == prefix.size()) {
                        continue;
                    }
                    if (std::ranges::any_of(excluded,
                                            [&name](const std::string& skip) { return name.starts_with(skip); })) {
                        continue;
                    }
                    out.push_back(name.substr(prefix.size()));
                }
                return out;
            },
        .read = [whole, prefix](std::string_view path) -> std::optional<std::vector<std::uint8_t>> {
            return whole.read ? whole.read(prefix + std::string{path}) : std::nullopt;
        },
    };
}

PackContents::PackContents(std::string packId, PackFiles files, const std::vector<std::string>& overlays,
                           const wire::common::Logger& logger)
    : packId_(std::move(packId)), files_(std::move(files)) {
    const std::vector<std::string> all = files_.list ? files_.list() : std::vector<std::string>{};
    // The pack first, then each overlay over it: a later one replaces what an earlier one supplied.
    std::vector<std::string> layers{""};
    for (const std::string& overlay : overlays) {
        layers.push_back(overlay + "/");
    }
    std::set<std::string> refusedNamespaces;
    for (const std::string& layer : layers) {
        const std::string root = layer + std::string{kDataRoot};
        for (const std::string& path : all) {
            if (!path.starts_with(root)) {
                continue;
            }
            const std::string_view rest = std::string_view{path}.substr(root.size());
            const std::size_t slash = rest.find('/');
            if (slash == std::string_view::npos) {
                continue;
            }
            const std::string_view nameSpace = rest.substr(0, slash);
            const std::string_view resource = rest.substr(slash + 1);
            if (!validNamespace(nameSpace)) {
                if (refusedNamespaces.emplace(nameSpace).second) {
                    logger.warn([&] {
                        return "Non [a-z0-9_.-] character in namespace " + std::string{nameSpace} + " in pack " +
                               packId_ + ", ignoring";
                    });
                }
                continue;
            }
            if (!validPath(resource)) {
                logger.warn([&] {
                    return "Invalid path in pack: " + std::string{nameSpace} + ":" + std::string{resource} +
                           ", ignoring";
                });
                continue;
            }
            namespaces_.emplace(nameSpace);
            index_.insert_or_assign(std::string{kDataRoot} + std::string{rest}, path);
        }
    }
}

std::optional<std::vector<std::uint8_t>> PackContents::read(const std::string& name) const {
    const auto found = index_.find(name);
    if (found == index_.end() || !files_.read) {
        return std::nullopt;
    }
    return files_.read(found->second);
}

} // namespace basalt::server::runtime::datapack
