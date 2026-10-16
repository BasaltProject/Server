#pragma once

#include <basalt/server/plugins/PluginEvents.hpp>
#include <basalt/wire/lua/LuaArgs.hpp>
#include <basalt/wire/lua/LuaSandbox.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace basalt::server::plugins {
class PluginEventView {
public:
    using OnCancel = std::function<void(std::optional<std::string>)>;

    using Objects = std::function<bool(wire::lua::LuaArgs&, std::string_view name)>;

    using Resolve = std::function<std::string(std::string_view message)>;

    PluginEventView(const PluginEventView&) = delete;
    PluginEventView& operator=(const PluginEventView&) = delete;
    ~PluginEventView();

    [[nodiscard]] static std::unique_ptr<PluginEventView> create(wire::lua::LuaSandbox& sandbox, PluginEvent& event,
                                                                 OnCancel onCancel, Objects objects = {},
                                                                 Resolve resolve = {});

    void bind();

    void push(wire::lua::LuaArgs& lua) const;

private:
    PluginEventView(wire::lua::LuaSandbox& sandbox, PluginEvent& event, OnCancel onCancel, Objects objects,
                    Resolve resolve);

    friend struct ViewAccess;

    wire::lua::LuaSandbox* sandbox_;
    PluginEvent* event_;
    OnCancel onCancel_;
    Objects objects_;
    Resolve resolve_;
    std::int64_t generation_ = -1;
    std::int32_t tableRef_ = 0;
};
} // namespace basalt::server::plugins
