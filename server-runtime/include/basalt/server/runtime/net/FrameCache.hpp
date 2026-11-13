#pragma once

#include <basalt/server/runtime/net/SessionOutbox.hpp>

#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <span>
#include <unordered_map>
#include <vector>

namespace basalt::server::runtime::net {

/// Compressed frames keyed by the exact packet bytes, so equal packets are deflated once for every connection.
class FrameCache {
public:
    static constexpr std::size_t kBudgetBytes = 24UL * 1024UL * 1024UL;

    explicit FrameCache(std::size_t budgetBytes = kBudgetBytes) : budgetBytes_(budgetBytes) {}

    [[nodiscard]] static FrameCache& instance();

    /// Null when the packet cannot be framed. A hit is verified byte for byte, never by hash alone.
    [[nodiscard]] SessionOutbox::SharedFrame compressed(std::span<const std::uint8_t> packet, std::int32_t threshold,
                                                        std::int32_t level);

private:
    struct Entry {
        std::uint64_t hash;
        std::int32_t threshold;
        std::int32_t level;
        std::vector<std::uint8_t> packet;
        SessionOutbox::SharedFrame frame;
    };

    void insert(std::shared_ptr<const Entry> entry);

    std::size_t budgetBytes_;
    std::mutex mutex_;
    std::unordered_multimap<std::uint64_t, std::shared_ptr<const Entry>> byHash_;
    std::deque<std::shared_ptr<const Entry>> order_;
    std::size_t bytes_ = 0;
};

} // namespace basalt::server::runtime::net
