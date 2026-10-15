#pragma once

#include <basalt/server/runtime/datapack/DataPackSelection.hpp>
#include <basalt/server/runtime/datapack/PackRepository.hpp>
#include <basalt/server/runtime/datapack/ResourceStack.hpp>

#include <memory>
#include <string>
#include <vector>

namespace basalt::server::runtime::datapack {

class DataPacks {
public:
    struct Options {
        PackFiles jar;
        std::string worldRoot;
        /// parity: `initial-enabled-packs`/`initial-disabled-packs`.
        DataPackConfig initial;
        /// parity: `--safeMode`, vanilla alone whatever the world saved.
        bool safeMode = false;
        bool persist = true;
    };

    explicit DataPacks(Options options, wire::common::Logger logger = wire::common::Logger{"Datapacks"});

    [[nodiscard]] PackRepository& repository() noexcept { return repository_; }

    [[nodiscard]] const PackRepository& repository() const noexcept { return repository_; }

    [[nodiscard]] const WorldDataConfiguration& configuration() const noexcept { return configuration_; }

    [[nodiscard]] std::shared_ptr<const ResourceStack> resources() const noexcept { return resources_; }

    /// parity: `ReloadCommand.discoverNewPacks`.
    [[nodiscard]] std::vector<std::string> discoverNewPacks();

    [[nodiscard]] std::shared_ptr<const ResourceStack> open(const std::vector<std::string>& ids) const {
        return repository_.open(ids);
    }

    /// parity: the tail of `MinecraftServer.reloadResources`.
    void commit(const std::vector<std::string>& ids, std::shared_ptr<const ResourceStack> resources);

private:
    void save() const;

    Options options_;
    wire::common::Logger logger_;
    PackRepository repository_;
    WorldDataConfiguration configuration_;
    std::shared_ptr<const ResourceStack> resources_;
};

} // namespace basalt::server::runtime::datapack
