#include <basalt/server/runtime/net/FrameCache.hpp>
#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/protocol/PacketFraming.hpp>

#include <algorithm>
#include <cstring>
#include <utility>

namespace basalt::server::runtime::net {
namespace {

constexpr std::uint64_t kMixA = 0x9E3779B97F4A7C15ULL;
constexpr std::uint64_t kMixB = 0xC2B2AE3D27D4EB4FULL;

[[nodiscard]] std::uint64_t hashBytes(std::span<const std::uint8_t> bytes) {
    std::uint64_t first = kMixA ^ bytes.size();
    std::uint64_t second = kMixB;
    std::size_t at = 0;
    for (; at + 16 <= bytes.size(); at += 16) {
        std::uint64_t a = 0;
        std::uint64_t b = 0;
        std::memcpy(&a, bytes.data() + at, 8);
        std::memcpy(&b, bytes.data() + at + 8, 8);
        first = (first ^ a) * kMixB;
        first ^= first >> 29;
        second = (second ^ b) * kMixA;
        second ^= second >> 31;
    }
    for (; at < bytes.size(); ++at) {
        first = (first ^ bytes[at]) * kMixB;
    }
    std::uint64_t result = first ^ (second * kMixA);
    result ^= result >> 32;
    return result;
}

} // namespace

FrameCache& FrameCache::instance() {
    static FrameCache cache;
    return cache;
}

SessionOutbox::SharedFrame FrameCache::compressed(std::span<const std::uint8_t> packet, std::int32_t threshold,
                                                  std::int32_t level) {
    const std::uint64_t hash = hashBytes(packet);
    {
        const std::lock_guard<std::mutex> guard{mutex_};
        const auto [first, last] = byHash_.equal_range(hash);
        for (auto it = first; it != last; ++it) {
            const Entry& entry = *it->second;
            if (entry.threshold == threshold && entry.level == level && entry.packet.size() == packet.size() &&
                std::equal(packet.begin(), packet.end(), entry.packet.begin())) {
                return entry.frame;
            }
        }
    }
    wire::io::ByteBuffer framed{packet.size() / 2 + 16};
    if (!wire::protocol::packetFraming::writeCompressed(framed, packet, threshold, level).has_value()) {
        return nullptr;
    }
    auto entry =
        std::make_shared<const Entry>(Entry{.hash = hash,
                                            .threshold = threshold,
                                            .level = level,
                                            .packet = std::vector<std::uint8_t>(packet.begin(), packet.end()),
                                            .frame = std::make_shared<const SessionOutbox::Frame>(framed.toVector())});
    SessionOutbox::SharedFrame frame = entry->frame;
    insert(std::move(entry));
    return frame;
}

void FrameCache::insert(std::shared_ptr<const Entry> entry) {
    const std::size_t size = entry->packet.size() + entry->frame->size();
    if (size > budgetBytes_ / 4) {
        return;
    }
    const std::lock_guard<std::mutex> guard{mutex_};
    bytes_ += size;
    byHash_.emplace(entry->hash, entry);
    order_.push_back(std::move(entry));
    while (bytes_ > budgetBytes_ && !order_.empty()) {
        const std::shared_ptr<const Entry>& oldest = order_.front();
        const auto [first, last] = byHash_.equal_range(oldest->hash);
        for (auto it = first; it != last; ++it) {
            if (it->second == oldest) {
                byHash_.erase(it);
                break;
            }
        }
        bytes_ -= oldest->packet.size() + oldest->frame->size();
        order_.pop_front();
    }
}

} // namespace basalt::server::runtime::net
