#pragma once

#include "WasmState.hpp"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace basalt::server::plugins::wasm {
/// A chest window the plugin owns, shared by everyone who opens it.
struct Menu {
    std::string title;
    std::int32_t rows = 3;
    std::map<std::int32_t, PluginItem> items;
    std::map<std::int32_t, std::int32_t> tokens;
    std::map<std::string, PluginHandle> opened;
};

class MenuTable {
public:
    [[nodiscard]] std::int32_t create(std::string title, std::int32_t rows);

    [[nodiscard]] Menu* find(std::int32_t handle);

    void closeAll(State& state);

private:
    std::vector<std::unique_ptr<Menu>> menus_;
};
} // namespace basalt::server::plugins::wasm
