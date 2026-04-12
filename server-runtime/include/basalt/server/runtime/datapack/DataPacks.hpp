#pragma once

#include <basalt/server/runtime/datapack/DataPackSelection.hpp>
#include <basalt/server/runtime/datapack/PackRepository.hpp>
#include <basalt/server/runtime/datapack/ResourceStack.hpp>

#include <memory>
#include <string>
#include <vector>

namespace basalt::server::runtime::datapack {

/// The server's packs: the repository, the world's saved selection and the tree it opens to.
///
/// Vanilla keeps one selection per save, in `level.dat`. Here a save is the default world, so the
/// selection lives beside its `level.nbt` and its `datapacks/` folder is the one scanned.
class DataPacks {
public:
    struct Options {
        /// The whole unpacked jar, as the repository reads it.
        PackFiles jar;
        /// The default world's directory.
        std::string worldRoot;
        /// What a brand new world starts from. parity: `initial-enabled-packs`/`initial-disabled-packs`.
        DataPackConfig initial;
        /// parity: `--safeMode`, vanilla alone whatever the world saved.
        bool safeMode = false;
        /// Whether the selection is written back. Off for the fallback after packs failed to load,
        /// so the world keeps the list its operator chose.
        bool persist = true;
    };

    explicit DataPacks(Options options, wire::common::Logger logger = wire::common::Logger{"Datapacks"});

    [[nodiscard]] PackRepository& repository() noexcept { return repository_; }

    [[nodiscard]] const PackRepository& repository() const noexcept { return repository_; }

    [[nodiscard]] const WorldDataConfiguration& configuration() const noexcept { return configuration_; }

    /// The tree of the packs in use right now.
    [[nodiscard]] std::shared_ptr<const ResourceStack> resources() const noexcept { return resources_; }

    /// parity: `ReloadCommand.discoverNewPacks`: rescans, and adds to the selection every pack the
    /// world has not disabled.
    [[nodiscard]] std::vector<std::string> discoverNewPacks();

    /// Opens a selection without making it current, so a failed load leaves everything as it was.
    [[nodiscard]] std::shared_ptr<const ResourceStack> open(const std::vector<std::string>& ids) const {
        return repository_.open(ids);
    }

    /// Makes a selection current and saves it. parity: the tail of `MinecraftServer.reloadResources`.
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
