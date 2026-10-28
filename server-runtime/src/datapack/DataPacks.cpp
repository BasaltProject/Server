#include <basalt/server/runtime/datapack/DataPacks.hpp>
#include <basalt/wire/io/FileIO.hpp>

#include <algorithm>
#include <utility>

namespace basalt::server::runtime::datapack {

DataPacks::DataPacks(Options options, wire::common::Logger logger)
    : options_(std::move(options)), logger_(std::move(logger)),
      repository_(PackRepository::Sources{.jar = options_.jar,
                                          .worldPacks = options_.worldRoot.empty() ? std::string{}
                                                                                   : options_.worldRoot + "/datapacks"},
                  logger_) {
    const std::optional<WorldDataConfiguration> saved = WorldDataConfiguration::read(options_.worldRoot);
    // parity: a save without `level.dat` is a new world.
    const bool initMode = !saved.has_value() && !wire::io::fileIO::exists(options_.worldRoot + "/level.nbt");
    WorldDataConfiguration initial;
    if (saved.has_value()) {
        initial = *saved;
    } else if (initMode) {
        initial.dataPacks = options_.initial;
    }
    configuration_ = configurePackRepository(repository_, initial, initMode, options_.safeMode, logger_);
    resources_ = repository_.openSelected();
    logger_.info([this] {
        std::string named;
        for (const Pack* pack : repository_.selectedPacks()) {
            named += (named.empty() ? "" : ", ") + pack->id + (pack->isCompatible() ? "" : " (incompatible)");
        }
        return "Loaded data packs: " + named;
    });
    if (!options_.worldRoot.empty() && options_.persist) {
        save();
    }
}

std::vector<std::string> DataPacks::discoverNewPacks() {
    repository_.reload();
    std::vector<std::string> selected = repository_.selectedIds();
    for (const std::string& id : repository_.availableIds()) {
        if (!std::ranges::contains(configuration_.dataPacks.disabled, id) && !std::ranges::contains(selected, id)) {
            selected.push_back(id);
        }
    }
    return selected;
}

void DataPacks::commit(const std::vector<std::string>& ids, std::shared_ptr<const ResourceStack> resources) {
    resources_ = std::move(resources);
    repository_.setSelected(ids);
    configuration_.dataPacks = selectedPacks(repository_, true);
    if (!options_.worldRoot.empty() && options_.persist) {
        save();
    }
}

void DataPacks::save() const {
    if (!configuration_.writeTo(options_.worldRoot)) {
        logger_.warn([this] { return "Could not save the data pack selection to " + options_.worldRoot; });
    }
}

} // namespace basalt::server::runtime::datapack
