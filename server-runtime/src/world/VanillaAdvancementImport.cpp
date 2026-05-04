#include <basalt/metamorph/datafix/ProgressData.hpp>
#include <basalt/server/runtime/world/VanillaAdvancementImport.hpp>
#include <basalt/server/storage/ProgressRepository.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>
#include <basalt/wire/common/json/Json.hpp>

#include <charconv>
#include <map>

namespace basalt::server::runtime::world {
namespace {

using wire::common::JsonObject;
using wire::common::JsonValue;

constexpr std::int64_t kMillisPerSecond = 1000;
constexpr std::int64_t kSecondsPerDay = 86400;

[[nodiscard]] std::optional<std::int64_t> number(std::string_view text) {
    std::int64_t value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    return error == std::errc{} && end == text.data() + text.size() ? std::optional{value} : std::nullopt;
}

[[nodiscard]] std::int64_t daysFromCivil(std::int64_t year, std::int64_t month, std::int64_t day) {
    year -= month <= 2 ? 1 : 0;
    const std::int64_t era = (year >= 0 ? year : year - 399) / 400;
    const std::int64_t yearOfEra = year - (era * 400);
    const std::int64_t dayOfYear = ((153 * (month + (month > 2 ? -3 : 9))) + 2) / 5 + day - 1;
    const std::int64_t dayOfEra = (yearOfEra * 365) + (yearOfEra / 4) - (yearOfEra / 100) + dayOfYear;
    return (era * 146097) + dayOfEra - 719468;
}

} // namespace

std::optional<std::int64_t> parseObtainedTime(std::string_view text) {
    if (text.size() != 25 || text[4] != '-' || text[7] != '-' || text[10] != ' ' || text[13] != ':' ||
        text[16] != ':' || text[19] != ' ' || (text[20] != '+' && text[20] != '-')) {
        return std::nullopt;
    }
    const auto year = number(text.substr(0, 4));
    const auto month = number(text.substr(5, 2));
    const auto day = number(text.substr(8, 2));
    const auto hour = number(text.substr(11, 2));
    const auto minute = number(text.substr(14, 2));
    const auto second = number(text.substr(17, 2));
    const auto offsetHours = number(text.substr(21, 2));
    const auto offsetMinutes = number(text.substr(23, 2));
    if (!year || !month || !day || !hour || !minute || !second || !offsetHours || !offsetMinutes) {
        return std::nullopt;
    }
    const std::int64_t offset = ((*offsetHours * 60) + *offsetMinutes) * 60 * (text[20] == '-' ? -1 : 1);
    const std::int64_t seconds =
        (daysFromCivil(*year, *month, *day) * kSecondsPerDay) + (*hour * 3600) + (*minute * 60) + *second - offset;
    return seconds * kMillisPerSecond;
}

std::int32_t importVanillaAdvancements(std::string_view json, std::string_view uuid,
                                       storage::ProgressRepository& progress) {
    const std::expected<JsonValue, wire::common::JsonError> root = wire::common::parseJson(json);
    const auto* advancements = root.has_value() ? root->get<JsonObject>() : nullptr;
    if (advancements == nullptr) {
        return 0;
    }
    std::int32_t taken = 0;
    const JsonValue* versionValue = advancements->find("DataVersion");
    const auto* versionNumber = versionValue == nullptr ? nullptr : versionValue->get<std::int64_t>();
    const auto fromVersion = static_cast<std::int32_t>(versionNumber == nullptr ? 0 : *versionNumber);
    for (std::size_t index = 0; index < advancements->names().size(); ++index) {
        const auto* entry = advancements->values()[index].get<JsonObject>();
        const JsonValue* criteria = entry == nullptr ? nullptr : entry->find("criteria");
        const auto* named = criteria == nullptr ? nullptr : criteria->get<JsonObject>();
        if (named == nullptr) {
            continue;
        }
        for (std::size_t criterion = 0; criterion < named->names().size(); ++criterion) {
            const auto* when = named->values()[criterion].get<std::string>();
            const std::optional<std::int64_t> obtained = when == nullptr ? std::nullopt : parseObtainedTime(*when);
            const std::string advancement =
                metamorph::datafix::advancementRenamed(advancements->names()[index], fromVersion);
            progress.grantCriterion(
                uuid, advancement,
                metamorph::datafix::criterionRenamed(advancement, named->names()[criterion], fromVersion),
                obtained.value_or(0));
            taken++;
        }
    }
    return taken;
}

std::int32_t importVanillaStats(std::string_view json, storage::StoredStats& stats) {
    const std::expected<JsonValue, wire::common::JsonError> root = wire::common::parseJson(json);
    const auto* file = root.has_value() ? root->get<JsonObject>() : nullptr;
    if (file == nullptr) {
        return 0;
    }
    const JsonValue* versionValue = file->find("DataVersion");
    const auto* versionNumber = versionValue == nullptr ? nullptr : versionValue->get<std::int64_t>();
    const auto fromVersion = static_cast<std::int32_t>(versionNumber == nullptr ? 0 : *versionNumber);
    metamorph::datafix::StatCounters counters;
    const JsonValue* typed = file->find("stats");
    if (const auto* byType = typed == nullptr ? nullptr : typed->get<JsonObject>(); byType != nullptr) {
        for (std::size_t type = 0; type < byType->names().size(); ++type) {
            const auto* keys = byType->values()[type].get<JsonObject>();
            for (std::size_t key = 0; keys != nullptr && key < keys->names().size(); ++key) {
                if (const auto* count = keys->values()[key].get<std::int64_t>(); count != nullptr) {
                    counters[byType->names()[type]][keys->names()[key]] = *count;
                }
            }
        }
    } else {
        std::map<std::string, std::int64_t> flat;
        for (std::size_t key = 0; key < file->names().size(); ++key) {
            if (const auto* count = file->values()[key].get<std::int64_t>(); count != nullptr) {
                flat[file->names()[key]] = *count;
            }
        }
        counters = metamorph::datafix::statsOfLegacy(flat);
    }
    metamorph::datafix::upgradeStats(counters, fromVersion);
    std::int32_t taken = 0;
    for (const auto& [type, keys] : counters) {
        for (const auto& [key, count] : keys) {
            stats.counters[type + "/" + key] = count;
            taken++;
        }
    }
    return taken;
}

} // namespace basalt::server::runtime::world
