#include <basalt/server/runtime/entity/BlockTriggers.hpp>

#include <array>
#include <string_view>

namespace basalt::server::runtime::entity::blockTriggers {
namespace {

/// parity: the five overrides of `isIgnoringBlockTriggers` that answer true, `Display`'s three
/// subclasses written out. A bat is one of them, which is the surprise: it is the only mob that
/// walks over a plate without pressing it.
constexpr std::array<std::string_view, 7> kIgnored{
    "ominous_item_spawner", "interaction", "block_display", "item_display", "text_display", "marker", "bat"};

} // namespace

bool ignored(const wire::common::Identifier& type) {
    if (type.namespaceName() != wire::common::Identifier::kVanillaNamespace) {
        return false;
    }
    for (const std::string_view name : kIgnored) {
        if (type.path() == name) {
            return true;
        }
    }
    return false;
}

} // namespace basalt::server::runtime::entity::blockTriggers
