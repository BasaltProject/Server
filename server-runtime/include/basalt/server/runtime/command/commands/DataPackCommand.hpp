#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>
#include <basalt/server/runtime/datapack/DataPacks.hpp>

#include <functional>
#include <string>
#include <vector>

namespace basalt::server::runtime::command::commands {

struct DataPackControl {
    datapack::DataPacks* packs = nullptr;
    /// parity: `MinecraftServer.reloadResources`.
    std::function<bool(const std::vector<std::string>& selected)> reload;
};

/// parity: `DataPackCommand`.
class DataPackCommand final : public Command {
public:
    explicit DataPackCommand(DataPackControl control);

    void build(const CommandNodeBuilder& root) const override;

private:
    DataPackControl control_;
};

} // namespace basalt::server::runtime::command::commands
