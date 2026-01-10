#include <basalt/server/assets/VersionMetadata.hpp>
#include <basalt/wire/common/json/Json.hpp>
#include <basalt/wire/io/FileIO.hpp>

#include <charconv>
#include <cstdint>
#include <format>

namespace basalt::server::assets {
namespace {

using wire::common::JsonObject;
using wire::common::JsonValue;

[[nodiscard]] std::string artifactJson(const DownloadArtifact& artifact) {
    // The size goes back out quoted, exactly as the Kotlin wrote it: the reader takes either.
    return std::format(R"({{"url":"{}","sha1":"{}","size":"{}"}})", artifact.url, artifact.sha1, artifact.size);
}

/// piston-meta writes the size as a number; a cached copy of ours writes it as a string.
[[nodiscard]] std::optional<std::int64_t> asLong(const JsonValue* value) {
    if (value == nullptr) {
        return std::nullopt;
    }
    if (const std::int64_t* whole = value->get<std::int64_t>(); whole != nullptr) {
        return *whole;
    }
    if (const std::string* text = value->get<std::string>(); text != nullptr) {
        std::int64_t parsed = 0;
        if (std::from_chars(text->data(), text->data() + text->size(), parsed).ec == std::errc{}) {
            return parsed;
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::expected<DownloadArtifact, AssetError> parseArtifact(const JsonObject& downloads,
                                                                        std::string_view key) {
    const JsonValue* value = downloads.find(key);
    const JsonObject* artifact = value == nullptr ? nullptr : value->get<JsonObject>();
    if (artifact == nullptr) {
        return std::unexpected(AssetError{std::string{"version metadata has no '"} + std::string{key} + "' download"});
    }
    const JsonValue* url = artifact->find("url");
    const JsonValue* sha1 = artifact->find("sha1");
    const std::optional<std::int64_t> size = asLong(artifact->find("size"));
    if (url == nullptr || url->get<std::string>() == nullptr || sha1 == nullptr ||
        sha1->get<std::string>() == nullptr || !size.has_value()) {
        return std::unexpected(
            AssetError{std::string{"version metadata's '"} + std::string{key} + "' download is malformed"});
    }
    return DownloadArtifact{.url = *url->get<std::string>(), .sha1 = *sha1->get<std::string>(), .size = *size};
}

} // namespace

std::string VersionMetadata::toJson() const {
    return std::format(R"({{"downloads":{{"client":{},"server":{}}}}})", artifactJson(client), artifactJson(server));
}

std::expected<VersionMetadata, AssetError> versionManifest::parse(std::string_view versionId, std::string_view json) {
    const std::expected<JsonValue, wire::common::JsonError> parsed = wire::common::parseJson(json);
    if (!parsed.has_value()) {
        return std::unexpected(AssetError{"version metadata is not readable JSON"});
    }
    const JsonObject* root = parsed->get<JsonObject>();
    const JsonValue* downloadsValue = root == nullptr ? nullptr : root->find("downloads");
    const JsonObject* downloads = downloadsValue == nullptr ? nullptr : downloadsValue->get<JsonObject>();
    if (downloads == nullptr) {
        return std::unexpected(AssetError{"version metadata has no downloads"});
    }
    const std::expected<DownloadArtifact, AssetError> client = parseArtifact(*downloads, "client");
    if (!client.has_value()) {
        return std::unexpected(client.error());
    }
    const std::expected<DownloadArtifact, AssetError> server = parseArtifact(*downloads, "server");
    if (!server.has_value()) {
        return std::unexpected(server.error());
    }
    return VersionMetadata{.id = std::string{versionId}, .client = *client, .server = *server};
}

std::expected<VersionMetadata, AssetError> versionManifest::resolve(std::string_view versionId, const Fetcher& fetch) {
    const std::optional<std::string> manifestBody = fetch(kVersionManifestUrl);
    if (!manifestBody.has_value()) {
        return std::unexpected(AssetError{"could not fetch the version manifest"});
    }
    const std::expected<JsonValue, wire::common::JsonError> manifest = wire::common::parseJson(*manifestBody);
    const JsonObject* root = manifest.has_value() ? manifest->get<JsonObject>() : nullptr;
    const JsonValue* versionsValue = root == nullptr ? nullptr : root->find("versions");
    const std::vector<JsonValue>* versions =
        versionsValue == nullptr ? nullptr : versionsValue->get<std::vector<JsonValue>>();
    if (versions == nullptr) {
        return std::unexpected(AssetError{"the version manifest is malformed"});
    }
    for (const JsonValue& element : *versions) {
        const JsonObject* entry = element.get<JsonObject>();
        if (entry == nullptr) {
            continue;
        }
        const JsonValue* id = entry->find("id");
        const std::string* name = id == nullptr ? nullptr : id->get<std::string>();
        if (name == nullptr || *name != versionId) {
            continue;
        }
        const JsonValue* url = entry->find("url");
        const std::string* metadataUrl = url == nullptr ? nullptr : url->get<std::string>();
        if (metadataUrl == nullptr) {
            return std::unexpected(
                AssetError{std::string{"the manifest entry for "} + std::string{versionId} + " has no metadata url"});
        }
        const std::optional<std::string> metadataBody = fetch(*metadataUrl);
        if (!metadataBody.has_value()) {
            return std::unexpected(
                AssetError{std::string{"could not fetch the metadata for "} + std::string{versionId}});
        }
        return parse(versionId, *metadataBody);
    }
    return std::unexpected(AssetError{std::string{"Unknown Minecraft version: "} + std::string{versionId}});
}

std::expected<VersionMetadata, AssetError> versionManifest::resolveCached(std::string_view versionId,
                                                                          const Fetcher& fetch,
                                                                          const std::optional<std::string>& cacheRoot) {
    std::optional<std::string> path;
    if (cacheRoot.has_value()) {
        path = *cacheRoot + "/" + std::string{versionId} + "/metadata.json";
        if (const std::optional<std::string> cached = wire::io::fileIO::readText(*path); cached.has_value()) {
            if (std::expected<VersionMetadata, AssetError> parsed = parse(versionId, *cached); parsed.has_value()) {
                return parsed;
            }
        }
    }
    const std::expected<VersionMetadata, AssetError> resolved = resolve(versionId, fetch);
    if (resolved.has_value() && path.has_value()) {
        wire::io::fileIO::createDirectories(*cacheRoot + "/" + std::string{versionId});
        wire::io::fileIO::writeText(*path, resolved->toJson());
    }
    return resolved;
}

} // namespace basalt::server::assets
