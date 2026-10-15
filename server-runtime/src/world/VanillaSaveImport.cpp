#include <basalt/server/runtime/world/VanillaAdvancementImport.hpp>
#include <basalt/server/runtime/world/VanillaSaveImport.hpp>
#include <basalt/server/storage/PlayerRepository.hpp>
#include <basalt/server/storage/VanillaPlayerData.hpp>
#include <basalt/wire/common/json/Json.hpp>
#include <basalt/wire/io/Compression.hpp>
#include <basalt/wire/io/FileIO.hpp>

#include <cstdint>
#include <vector>

namespace basalt::server::runtime::world {
namespace {

using wire::common::JsonObject;
using wire::common::JsonValue;

[[nodiscard]] const std::string* stringAt(const JsonObject& entry, std::string_view key) {
    const JsonValue* value = entry.find(key);
    return value == nullptr ? nullptr : value->get<std::string>();
}

} // namespace

std::expected<ImportReport, std::string> importVanillaSave(const std::string& savePath,
                                                           storage::PlayerRepository& repository,
                                                           const std::string& worldName, std::int64_t nowEpochSeconds,
                                                           const wire::common::Logger& logger,
                                                           storage::ProgressRepository* progress) {
    const std::string cachePath = savePath + "/usercache.json";
    const std::optional<std::string> cacheText = wire::io::fileIO::readText(cachePath);
    if (!cacheText.has_value()) {
        return std::unexpected(cachePath + " not found: is " + savePath + " a vanilla server directory?");
    }
    const std::expected<JsonValue, wire::common::JsonError> parsed = wire::common::parseJson(*cacheText);
    const auto* entries = parsed.has_value() ? parsed->get<std::vector<JsonValue>>() : nullptr;
    if (entries == nullptr) {
        return std::unexpected(cachePath + " is not the JSON array vanilla writes");
    }

    ImportReport report;
    for (const JsonValue& element : *entries) {
        const auto* entry = element.get<JsonObject>();
        if (entry == nullptr) {
            continue;
        }
        const std::string* uuid = stringAt(*entry, "uuid");
        const std::string* name = stringAt(*entry, "name");
        if (uuid == nullptr || name == nullptr) {
            continue;
        }
        const std::optional<std::vector<std::uint8_t>> bytes =
            wire::io::fileIO::readBytes(savePath + "/playerdata/" + *uuid + ".dat");
        if (!bytes.has_value()) {
            report.skipped++;
            continue;
        }
        const std::expected<storage::StoredPlayer, storage::StorageFormatError> player =
            storage::vanillaPlayerData::read(
                *uuid, *name, *bytes,
                [](std::span<const std::uint8_t> source) {
                    const std::expected<std::vector<std::uint8_t>, wire::io::CompressionError> plain =
                        wire::io::gzip::decompress(source);
                    return plain.has_value() ? *plain : std::vector<std::uint8_t>{};
                },
                nowEpochSeconds, worldName);
        if (!player.has_value()) {
            logger.warn([&] { return "Skipping " + *name + " (" + *uuid + "): its player data could not be read."; });
            report.failed++;
            continue;
        }
        repository.save(*player);
        if (progress != nullptr) {
            if (const std::optional<std::string> advancements =
                    wire::io::fileIO::readText(savePath + "/advancements/" + *uuid + ".json");
                advancements.has_value()) {
                static_cast<void>(importVanillaAdvancements(*advancements, *uuid, *progress));
            }
        }
        report.imported++;
    }

    logger.info([&] {
        return "Imported " + std::to_string(report.imported) + " player(s) from " + savePath + " (" +
               std::to_string(report.skipped) + " without data, " + std::to_string(report.failed) + " unreadable).";
    });
    return report;
}

} // namespace basalt::server::runtime::world
