#pragma once

#include <basalt/server/runtime/config/AnimationsConfig.hpp>
#include <basalt/server/runtime/config/CommandsConfig.hpp>
#include <basalt/server/runtime/config/ConfigDirectory.hpp>
#include <basalt/server/runtime/config/GroupsConfig.hpp>
#include <basalt/server/runtime/config/ServerListConfig.hpp>
#include <basalt/server/runtime/config/UiConfigs.hpp>
#include <basalt/wire/common/logging/Logger.hpp>

#include <array>
#include <expected>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

namespace basalt::server::runtime::config {

/// The configuration that is read on every use, as one immutable set.
///
/// One snapshot swapped atomically rather than five fields swapped one at a time: a reload that
/// replaced groups and then commands would, for the instant between them, be running with the new
/// permissions and the old commands. Everything reads through `current`, so a reader either sees
/// the whole old set or the whole new one.
struct ConfigSnapshot {
    GroupsConfig groups;
    ServerListConfig serverList;
    CommandsConfig commands;
    AnimationsConfig animations;
    UiConfigs ui;
};

/// What `/reload` replaces.
///
/// The line between what reloads and what does not is not a judgement call, it is a property of the
/// code: a file whose values are read every time they are used can be swapped under a running
/// server, and a file whose values were read once at startup and turned into a socket, a thread
/// pool or a loaded world cannot. Reload does the first and refuses the second by name, which is
/// the difference between this and the reload command everyone learned to distrust.
class LiveConfigs {
public:
    /// The files a reload re-reads. `server.toml` and `worlds.toml` are deliberately not among
    /// them.
    static constexpr std::array<std::string_view, 7> kReloadableFiles{
        GroupsConfig::kFileName,  ServerListConfig::kFileName, CommandsConfig::kFileName, AnimationsConfig::kFileName,
        TabListConfig::kFileName, ScoreboardConfig::kFileName, BossBarConfig::kFileName};

    explicit LiveConfigs(ConfigSnapshot initial)
        : snapshot_(std::make_shared<const ConfigSnapshot>(std::move(initial))) {}

    /// The whole set as it is right now. Held by the caller for as long as it reads from it, so a
    /// reload in the middle of a tick cannot pull it out from under them.
    ///
    /// Behind a mutex rather than an atomic shared pointer, which this standard library does not
    /// carry yet: what it guards is one pointer copy, and a reload is not something that happens
    /// per tick.
    [[nodiscard]] std::shared_ptr<const ConfigSnapshot> current() const {
        const std::lock_guard<std::mutex> lock{mutex_};
        return snapshot_;
    }

    void replace(ConfigSnapshot next) {
        // Built before the lock is taken: the copy is the expensive half and nobody reading needs
        // to wait for it.
        std::shared_ptr<const ConfigSnapshot> replacement = std::make_shared<const ConfigSnapshot>(std::move(next));
        const std::lock_guard<std::mutex> lock{mutex_};
        snapshot_ = std::move(replacement);
    }

    /// Reads all of them, or none.
    ///
    /// A file that does not parse is the whole read failing, and the caller keeps what it already
    /// had: a reload that applied the three files it managed to read and gave up on the fourth
    /// would leave a server in a state no file describes.
    [[nodiscard]] static std::expected<ConfigSnapshot, std::string>
    read(const ConfigDirectory& directory = ConfigDirectory{},
         const wire::common::Logger& logger = wire::common::Logger{"Config"});

private:
    mutable std::mutex mutex_;
    std::shared_ptr<const ConfigSnapshot> snapshot_;
};

} // namespace basalt::server::runtime::config
