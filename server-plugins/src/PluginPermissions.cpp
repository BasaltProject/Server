#include <basalt/server/plugins/PluginPermissions.hpp>

namespace basalt::server::plugins::permissions {
namespace {
class Unavailable final : public PluginPermissions {
public:
    PluginHandle provide(std::string_view, std::int32_t, Check, Meta) override { return {}; }

    void declare(std::string_view, std::int32_t, std::span<const std::string_view>, std::span<const bool>) override {}

    void undeclare(std::string_view) override {}

    std::int32_t check(std::string_view, std::string_view) override { return kUndefined; }

    std::optional<std::string> meta(std::string_view, std::string_view) override { return std::nullopt; }

    void changed() override {}
};
} // namespace

PluginPermissions& PluginPermissions::unavailable() {
    static Unavailable kNone;
    return kNone;
}
} // namespace basalt::server::plugins::permissions
