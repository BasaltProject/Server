#pragma once

#include <cstdint>
#include <functional>
#include <vector>

namespace basalt::server::runtime::entity {

class IntHashOrder {
public:
    IntHashOrder();

    void insert(std::int32_t key);

    void remove(std::int32_t key);

    [[nodiscard]] std::int32_t size() const noexcept { return size_; }

    [[nodiscard]] std::vector<std::int32_t> order() const;

    void iterate(const std::function<bool(std::int32_t key)>& visit);

private:
    [[nodiscard]] static std::uint32_t mix(std::int32_t key);
    void shiftKeys(std::int32_t pos, std::vector<std::int32_t>* wrapped);
    void rehash(std::int32_t capacity);

    std::vector<std::int32_t> keys_;
    std::int32_t mask_ = 0;
    std::int32_t size_ = 0;
    std::int32_t maxFill_ = 0;
};

} // namespace basalt::server::runtime::entity
