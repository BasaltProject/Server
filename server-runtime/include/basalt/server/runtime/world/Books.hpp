#pragma once

#include <basalt/server/storage/StoredPlayer.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

namespace books {

inline constexpr std::size_t kMaxPages = 100;
inline constexpr std::size_t kMaxPageLength = 1024;
inline constexpr std::size_t kMaxTitleLength = 32;

[[nodiscard]] bool isWritable(const storage::StoredItemStack& book);

[[nodiscard]] std::vector<std::string> pagesOf(const storage::StoredItemStack& book);

enum class PageEdit : std::uint8_t { Replace, Add, Delete, Swap };

[[nodiscard]] bool editPage(std::vector<std::string>& pages, PageEdit what, std::size_t page, std::size_t other,
                            std::string_view text);

[[nodiscard]] bool writePages(storage::StoredItemStack& book, const std::vector<std::string>& pages);

[[nodiscard]] std::optional<storage::StoredItemStack> sign(const storage::StoredItemStack& book, std::string_view title,
                                                           std::string_view author);

} // namespace books

} // namespace basalt::server::runtime::world
