#include <basalt/server/assets/ServerJarAssets.hpp>
#include <basalt/wire/io/FileIO.hpp>
#include <basalt/wire/io/ZipReader.hpp>

#include <format>
#include <utility>

namespace basalt::server::assets {
namespace {

constexpr std::string_view kDataPrefix = "data/";
constexpr std::string_view kLangPrefix = "assets/minecraft/lang/";
constexpr std::string_view kVanillaPrefix = "data/minecraft/";

/// Bumped whenever the set of entries unpacked below changes, so existing caches redo it.
constexpr std::string_view kUnpackFormat = "v2-lang";

[[nodiscard]] std::vector<std::string> splitLines(std::string_view text) {
    std::vector<std::string> lines;
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t end = std::min(text.find('\n', start), text.size());
        std::string_view line = text.substr(start, end - start);
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }
        if (!line.empty()) {
            lines.emplace_back(line);
        }
        if (end == text.size()) {
            break;
        }
        start = end + 1;
    }
    return lines;
}

[[nodiscard]] std::string join(const std::vector<std::string>& lines) {
    std::string joined;
    for (const std::string& line : lines) {
        if (!joined.empty()) {
            joined += '\n';
        }
        joined += line;
    }
    return joined;
}

} // namespace

std::string ServerJarAssets::pathFor(std::string_view name) const {
    if (name.starts_with(kVanillaPrefix)) {
        name.remove_prefix(kVanillaPrefix.size());
    }
    return dataRoot_ + "/" + std::string{name};
}

std::expected<ServerJarAssets, AssetError> ServerJarAssets::open(std::string_view versionId, std::string dataRoot,
                                                                 const Source& source, wire::common::Logger logger) {
    const std::string marker = dataRoot + "/.unpacked";
    // The marker carries what was unpacked, not just which version: widening the set of entries
    // taken has to invalidate a cache written under the narrower one, or an existing install
    // silently keeps missing whatever was added.
    const std::string stamp = std::string{versionId} + " " + std::string{kUnpackFormat};
    if (const std::optional<std::string> written = wire::io::fileIO::readText(marker); written.has_value()) {
        std::string_view trimmed{*written};
        while (!trimmed.empty() && (trimmed.back() == '\n' || trimmed.back() == '\r' || trimmed.back() == ' ')) {
            trimmed.remove_suffix(1);
        }
        if (trimmed == stamp) {
            const std::optional<std::string> index = wire::io::fileIO::readText(dataRoot + "/.index");
            return ServerJarAssets{std::move(dataRoot),
                                   index.has_value() ? splitLines(*index) : std::vector<std::string>{},
                                   std::move(logger)};
        }
    }

    const std::expected<std::string, AssetError> archivePath = source();
    if (!archivePath.has_value()) {
        return std::unexpected(archivePath.error());
    }
    const std::optional<std::vector<std::uint8_t>> outerBytes = wire::io::fileIO::readBytes(*archivePath);
    if (!outerBytes.has_value()) {
        return std::unexpected(AssetError{std::format("{} data not found at {}", versionId, *archivePath)});
    }
    // Since 1.18 the published download is a bundler wrapping the real server archive one level
    // down, confirmed against the real 26.2 download.
    const std::string innerEntryName = std::format("META-INF/versions/{0}/server-{0}.jar", versionId);
    const std::expected<wire::io::ZipReader, wire::io::ZipError> outer = wire::io::ZipReader::open(*outerBytes);
    if (!outer.has_value()) {
        return std::unexpected(AssetError{std::format("{} is not a readable archive", *archivePath)});
    }
    const std::expected<std::vector<std::uint8_t>, wire::io::ZipError> innerBytes = outer->read(innerEntryName);
    if (!innerBytes.has_value()) {
        return std::unexpected(
            AssetError{std::format("{} does not contain {}'s data where expected", *archivePath, versionId)});
    }
    const std::expected<wire::io::ZipReader, wire::io::ZipError> inner = wire::io::ZipReader::open(*innerBytes);
    if (!inner.has_value()) {
        return std::unexpected(AssetError{std::format("{}'s inner archive is not readable", *archivePath)});
    }

    logger.info([&] { return std::format("Unpacking {} data into {}...", versionId, dataRoot); });
    ServerJarAssets built{std::move(dataRoot), {}, std::move(logger)};
    for (const std::string& name : inner->entryNames()) {
        if (name.ends_with('/')) {
            continue;
        }
        if (!name.starts_with(kDataPrefix) && !name.starts_with(kLangPrefix)) {
            continue;
        }
        const std::expected<std::vector<std::uint8_t>, wire::io::ZipError> bytes = inner->read(name);
        if (!bytes.has_value()) {
            continue;
        }
        const std::string path = built.pathFor(name);
        wire::io::fileIO::createDirectories(path.substr(0, path.find_last_of('/')));
        wire::io::fileIO::writeBytes(path, *bytes);
        built.entryNames_.push_back(name);
    }
    wire::io::fileIO::createDirectories(built.dataRoot_);
    wire::io::fileIO::writeText(built.dataRoot_ + "/.index", join(built.entryNames_));
    // Written last, so an interrupted unpack is redone rather than trusted.
    wire::io::fileIO::writeText(built.dataRoot_ + "/.unpacked", stamp);

    // The archive has served its only purpose. Keeping it would leave several times the size of what
    // was extracted sitting on disk forever, for data already on disk in a usable form.
    wire::io::fileIO::remove(*archivePath);
    wire::io::fileIO::remove(*archivePath + ".verified");
    built.logger_.info(
        [&] { return std::format("Unpacked {} {} data file(s).", built.entryNames_.size(), versionId); });
    return built;
}

std::optional<std::vector<std::uint8_t>> ServerJarAssets::readEntry(std::string_view name) const {
    return wire::io::fileIO::readBytes(pathFor(name));
}

std::expected<std::string, AssetError> ServerJarAssets::readText(std::string_view name) const {
    const std::optional<std::vector<std::uint8_t>> bytes = readEntry(name);
    if (!bytes.has_value()) {
        return std::unexpected(AssetError{std::format("'{}' is missing from the {} data", name, dataRoot_)});
    }
    return std::string{bytes->begin(), bytes->end()};
}

std::vector<std::string> ServerJarAssets::entriesUnder(std::string_view prefix) const {
    std::vector<std::string> found;
    for (const std::string& name : entryNames_) {
        if (name.starts_with(prefix)) {
            found.push_back(name);
        }
    }
    return found;
}

std::vector<wire::common::Identifier> ServerJarAssets::dataIds(std::string_view directory,
                                                               std::string_view extension) const {
    std::vector<wire::common::Identifier> ids;
    for (const std::string& entry : entriesUnder(kDataPrefix)) {
        const std::string_view rest = std::string_view{entry}.substr(kDataPrefix.size());
        const std::size_t slash = rest.find('/');
        if (slash == std::string_view::npos || slash == 0) {
            continue;
        }
        const std::string_view name = rest.substr(0, slash);
        const std::string wanted = std::string{name} + "/" + std::string{directory} + "/";
        if (!rest.starts_with(wanted) || !rest.ends_with(extension)) {
            continue;
        }
        const std::string_view path = rest.substr(wanted.size(), rest.size() - wanted.size() - extension.size());
        ids.push_back(wire::common::Identifier{std::string{name}, std::string{path}});
    }
    return ids;
}

} // namespace basalt::server::assets
