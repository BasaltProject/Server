#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>
#include <basalt/server/runtime/datapack/DataPacks.hpp>

#include <functional>
#include <string>
#include <vector>

namespace basalt::server::runtime::command::commands {

/// What `/reload` and `/datapack` need of the server's packs.
struct DataPackControl {
    datapack::DataPacks* packs = nullptr;
    /// parity: `MinecraftServer.reloadResources`: loads the selection and makes it current,
    /// answering whether it loaded. A failed load leaves everything as it was.
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
