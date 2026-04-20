#include <basalt/server/runtime/datapack/DataPackSelection.hpp>
#include <basalt/server/runtime/datapack/PackRepository.hpp>
#include <basalt/wire/io/FileIO.hpp>
#include <basalt/wire/nbt/NbtIO.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <algorithm>
#include <set>

namespace basalt::server::runtime::datapack {
namespace {

using wire::common::Identifier;
using wire::nbt::NbtCompound;
using wire::nbt::NbtList;
using wire::nbt::NbtTag;

[[nodiscard]] NbtTag stringList(const std::vector<std::string>& values) {
    NbtList list;
    for (const std::string& value : values) {
        static_cast<void>(list.push(NbtTag::ofString(value)));
    }
    return NbtTag::ofList(std::move(list));
}

[[nodiscard]] std::vector<std::string> stringsOf(const NbtList* list) {
    std::vector<std::string> values;
    if (list == nullptr) {
        return values;
    }
    for (const NbtTag& tag : *list) {
        if (const auto* text = tag.get<std::string>(); text != nullptr) {
            values.push_back(*text);
        }
    }
    return values;
}

[[nodiscard]] const std::vector<Identifier>& allFeatures() { return knownFeatures(); }

} // namespace

std::optional<WorldDataConfiguration> WorldDataConfiguration::read(const std::string& worldRoot) {
    const std::optional<std::vector<std::uint8_t>> bytes = wire::io::fileIO::readBytes(pathFor(worldRoot));
    if (!bytes.has_value()) {
        return std::nullopt;
    }
    const std::expected<wire::nbt::NamedTag, wire::nbt::NbtError> named = wire::nbt::nbtIO::read(*bytes);
    const NbtCompound* root = named.has_value() ? named->tag.get<NbtCompound>() : nullptr;
    if (root == nullptr) {
        return std::nullopt;
    }
    // parity: `lenientOptionalFieldOf` with the defaults, so a half-written file still loads.
    WorldDataConfiguration configuration;
    if (const NbtCompound* packs = root->getCompound("DataPacks"); packs != nullptr) {
        configuration.dataPacks.enabled = stringsOf(packs->getList("Enabled"));
        configuration.dataPacks.disabled = stringsOf(packs->getList("Disabled"));
    }
    if (const NbtList* features = root->getList("enabled_features"); features != nullptr) {
        configuration.enabledFeatures.clear();
        for (const std::string& flag : stringsOf(features)) {
            const std::expected<Identifier, wire::common::IdentifierError> id = Identifier::parse(flag);
            if (id.has_value() && std::ranges::contains(knownFeatures(), *id)) {
                configuration.enabledFeatures.push_back(*id);
            }
        }
    }
    return configuration;
}

bool WorldDataConfiguration::writeTo(const std::string& worldRoot) const {
    NbtCompound packs;
    packs.set("Enabled", stringList(dataPacks.enabled));
    packs.set("Disabled", stringList(dataPacks.disabled));
    NbtCompound root;
    root.set("DataPacks", NbtTag::ofCompound(std::move(packs)));
    std::vector<std::string> flags;
    for (const Identifier& flag : enabledFeatures) {
        flags.push_back(flag.toString());
    }
    root.set("enabled_features", stringList(flags));
    wire::io::fileIO::createDirectories(worldRoot);
    return wire::io::fileIO::writeBytes(pathFor(worldRoot),
                                        wire::nbt::nbtIO::write(NbtTag::ofCompound(std::move(root))));
}

bool featuresAllowed(const std::vector<Identifier>& requested, const std::vector<Identifier>& allowed) {
    return std::ranges::all_of(requested,
                               [&allowed](const Identifier& flag) { return std::ranges::contains(allowed, flag); });
}

std::string missingFeatures(const std::vector<Identifier>& allowed, const std::vector<Identifier>& requested) {
    // parity: `FeatureFlagRegistry.toNames` is a set, so the names come out sorted.
    std::set<Identifier> missing;
    for (const Identifier& flag : requested) {
        if (!std::ranges::contains(allowed, flag)) {
            missing.insert(flag);
        }
    }
    std::string joined;
    for (const Identifier& flag : missing) {
        joined += joined.empty() ? flag.toString() : ", " + flag.toString();
    }
    return joined;
}

DataPackConfig selectedPacks(const PackRepository& repository, bool disableInactive) {
    DataPackConfig config;
    config.enabled = repository.selectedIds();
    config.disabled.clear();
    if (disableInactive) {
        for (const std::string& id : repository.availableIds()) {
            if (!repository.isSelected(id)) {
                config.disabled.push_back(id);
            }
        }
    }
    return config;
}

namespace {

/// parity: `enableForcedFeaturePacks`.
void enableForcedFeaturePacks(PackRepository& repository, const std::vector<Identifier>& forced,
                              const wire::common::Logger& logger) {
    std::vector<Identifier> missing;
    const std::vector<Identifier> provided = repository.requestedFeatures();
    for (const Identifier& flag : forced) {
        if (!std::ranges::contains(provided, flag)) {
            missing.push_back(flag);
        }
    }
    if (missing.empty()) {
        return;
    }
    std::vector<std::string> selected = repository.selectedIds();
    for (const Pack* pack : repository.availablePacks()) {
        if (missing.empty()) {
            break;
        }
        if (pack->source != PackSource::Feature || pack->requestedFeatures.empty() ||
            !featuresAllowed(pack->requestedFeatures, forced)) {
            continue;
        }
        const bool intersects = std::ranges::any_of(pack->requestedFeatures, [&missing](const Identifier& flag) {
            return std::ranges::contains(missing, flag);
        });
        if (!intersects || std::ranges::contains(selected, pack->id)) {
            continue;
        }
        selected.push_back(pack->id);
        logger.info(
            [pack] { return "Found feature pack ('" + pack->id + "') for requested feature, forcing to enabled"; });
        std::erase_if(missing,
                      [pack](const Identifier& flag) { return std::ranges::contains(pack->requestedFeatures, flag); });
    }
    repository.setSelected(selected);
}

[[nodiscard]] WorldDataConfiguration withSelection(PackRepository& repository, const std::vector<std::string>& selected,
                                                   const std::vector<Identifier>& forced, bool disableInactive,
                                                   const wire::common::Logger& logger) {
    repository.setSelected(selected);
    enableForcedFeaturePacks(repository, forced, logger);
    WorldDataConfiguration configuration;
    configuration.dataPacks = selectedPacks(repository, disableInactive);
    configuration.enabledFeatures = repository.requestedFeatures();
    for (const Identifier& flag : forced) {
        if (!std::ranges::contains(configuration.enabledFeatures, flag)) {
            configuration.enabledFeatures.push_back(flag);
        }
    }
    return configuration;
}

} // namespace

WorldDataConfiguration configurePackRepository(PackRepository& repository, const WorldDataConfiguration& initial,
                                               bool initMode, bool safeMode, const wire::common::Logger& logger) {
    const std::vector<Identifier> forced = initMode ? std::vector<Identifier>{} : initial.enabledFeatures;
    const std::vector<Identifier>& allowed = initMode ? allFeatures() : initial.enabledFeatures;
    repository.reload();
    if (safeMode) {
        return withSelection(repository, {"vanilla"}, forced, false, logger);
    }
    std::vector<std::string> selected;
    for (const std::string& id : initial.dataPacks.enabled) {
        if (!repository.isAvailable(id)) {
            logger.warn([&id] { return "Missing data pack " + id; });
        } else if (!std::ranges::contains(selected, id)) {
            selected.push_back(id);
        }
    }
    for (const Pack* pack : repository.availablePacks()) {
        if (std::ranges::contains(initial.dataPacks.disabled, pack->id)) {
            continue;
        }
        const bool isSelected = std::ranges::contains(selected, pack->id);
        const bool addsItself = pack->source != PackSource::Feature;
        if (!isSelected && addsItself) {
            if (featuresAllowed(pack->requestedFeatures, allowed)) {
                logger.info([pack] { return "Found new data pack " + pack->id + ", loading it automatically"; });
                selected.push_back(pack->id);
            } else {
                logger.info([&] {
                    return "Found new data pack " + pack->id + ", but can't load it due to missing features " +
                           missingFeatures(allowed, pack->requestedFeatures);
                });
            }
        }
        if (isSelected && !featuresAllowed(pack->requestedFeatures, allowed)) {
            logger.warn([&] {
                return "Pack " + pack->id + " requires features " + missingFeatures(allowed, pack->requestedFeatures) +
                       " that are not enabled for this world, disabling pack.";
            });
            std::erase(selected, pack->id);
        }
    }
    if (selected.empty()) {
        logger.info([] { return std::string{"No datapacks selected, forcing vanilla"}; });
        selected.emplace_back("vanilla");
    }
    return withSelection(repository, selected, forced, true, logger);
}

} // namespace basalt::server::runtime::datapack
