#pragma once

#include <basalt/server/storage/AccessRepository.hpp>
#include <basalt/server/storage/PlayerRepository.hpp>
#include <basalt/server/storage/PluginDataRepository.hpp>
#include <basalt/server/storage/ProgressRepository.hpp>
#include <basalt/server/storage/Sqlite.hpp>

#include <expected>
#include <memory>
#include <string>

namespace basalt::server::storage {
struct PlayerDatabaseSchemaError {
    std::string message;
};

class PlayerStore {
public:
    PlayerStore(const PlayerStore&) = delete;
    PlayerStore& operator=(const PlayerStore&) = delete;
    PlayerStore(PlayerStore&&) noexcept = default;
    PlayerStore& operator=(PlayerStore&&) noexcept = default;
    ~PlayerStore();

    [[nodiscard]] static std::expected<std::unique_ptr<PlayerStore>, PlayerDatabaseSchemaError>
    open(const std::string& path);

    [[nodiscard]] PlayerRepository& players() noexcept { return players_; }

    [[nodiscard]] AccessRepository& access() noexcept { return access_; }

    [[nodiscard]] ProgressRepository& progress() noexcept { return progress_; }

    [[nodiscard]] PluginDataRepository& pluginData() noexcept { return pluginData_; }

    [[nodiscard]] sql::Database& database() noexcept { return *database_; }

    [[nodiscard]] bool isOpen() const noexcept { return open_; }

    void close();

private:
    explicit PlayerStore(std::unique_ptr<sql::Database> database);

    std::unique_ptr<sql::Database> database_;
    PlayerRepository players_;
    AccessRepository access_;
    ProgressRepository progress_;
    PluginDataRepository pluginData_;
    bool open_ = true;
};
} // namespace basalt::server::storage
