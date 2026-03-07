#pragma once

#include <basalt/server/storage/StoredPlayer.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

/// What a player writes into a book. parity: `WritableBookContent` and `WrittenBookContent`, the two
/// components a book keeps its text in, and `signBook`, which swaps one for the other.
namespace books {

/// parity: the codec's own limits, past which the jar refuses the packet outright.
inline constexpr std::size_t kMaxPages = 100;
inline constexpr std::size_t kMaxPageLength = 1024;
inline constexpr std::size_t kMaxTitleLength = 32;

/// Whether that is a book somebody may still write in. parity: every `writable_book` is, empty or
/// not, because the component is one of the item's own.
[[nodiscard]] bool isWritable(const storage::StoredItemStack& book);

[[nodiscard]] std::vector<std::string> pagesOf(const storage::StoredItemStack& book);

/// One change to a page. parity: the four that edition sends, one page at a time.
enum class PageEdit : std::uint8_t { Replace, Add, Delete, Swap };

/// Applies one page change, filling in empty pages where the client skipped ahead. Answers whether
/// anything changed.
[[nodiscard]] bool editPage(std::vector<std::string>& pages, PageEdit what, std::size_t page, std::size_t other,
                            std::string_view text);

/// Writes the pages onto a writable book. Refused, and the book left alone, for anything else.
[[nodiscard]] bool writePages(storage::StoredItemStack& book, const std::vector<std::string>& pages);

/// The written book those pages make. parity: `signBook` keeps the pages, names the author and
/// marks it resolved; the writable content goes.
[[nodiscard]] std::optional<storage::StoredItemStack> sign(const storage::StoredItemStack& book, std::string_view title,
                                                           std::string_view author);

} // namespace books

} // namespace basalt::server::runtime::world
