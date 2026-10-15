#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::server::runtime::registry {

namespace vanillaTagData {

[[nodiscard]] const std::vector<std::pair<std::string, std::vector<std::string>>>& tagsByRegistry();

[[nodiscard]] const std::vector<std::string>& tagsOf(std::string_view registry);

} // namespace vanillaTagData

} // namespace basalt::server::runtime::registry
