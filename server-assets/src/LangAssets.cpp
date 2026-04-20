#include <basalt/server/assets/LangAssets.hpp>
#include <basalt/wire/io/FileIO.hpp>
#include <basalt/wire/io/ZipReader.hpp>

#include <cstdint>
#include <format>

namespace basalt::server::assets::langAssets {
namespace {
constexpr std::string_view kLangPrefix = "assets/minecraft/lang/";
} // namespace

std::expected<wire::text::LangTable, AssetError> load(const std::string& jarPath, std::string_view locale) {
    const std::optional<std::vector<std::uint8_t>> jarBytes = wire::io::fileIO::readBytes(jarPath);
    if (!jarBytes.has_value()) {
        return std::unexpected(AssetError{std::format("jar not found at {}", jarPath)});
    }
    const std::expected<wire::io::ZipReader, wire::io::ZipError> jar = wire::io::ZipReader::open(*jarBytes);
    if (!jar.has_value()) {
        return std::unexpected(AssetError{std::format("{} is not a readable archive", jarPath)});
    }
    const std::string entryName = std::string{kLangPrefix} + std::string{locale} + ".json";
    const std::expected<std::vector<std::uint8_t>, wire::io::ZipError> entryBytes = jar->read(entryName);
    if (!entryBytes.has_value()) {
        return std::unexpected(AssetError{std::format("'{}' not found in {}", entryName, jarPath)});
    }
    const std::expected<wire::text::LangTable, wire::common::JsonError> table = wire::text::parseLangTable(
        std::string_view{reinterpret_cast<const char*>(entryBytes->data()), entryBytes->size()});
    if (!table.has_value()) {
        return std::unexpected(
            AssetError{std::format("'{}' in {} is not a readable language file", entryName, jarPath)});
    }
    return *table;
}

wire::text::LangTable loadCachedOrEmpty(const std::string& versionDir, std::string_view locale) {
    const std::string path = versionDir + "/" + std::string{kLangPrefix} + std::string{locale} + ".json";
    const std::optional<std::string> content = wire::io::fileIO::readText(path);
    if (!content.has_value()) {
        return wire::text::LangTable{};
    }
    const std::expected<wire::text::LangTable, wire::common::JsonError> table = wire::text::parseLangTable(*content);
    return table.has_value() ? *table : wire::text::LangTable{};
}
} // namespace basalt::server::assets::langAssets
