#include "Nodes.hpp"

#include <basalt/server/world/densityfunction/SamplerContext.hpp>

#include <cmath>
#include <utility>

namespace basalt::server::world::densityfunction {
namespace {

/// parity: `BlockPos.asLong`, the key a cache cell remembers its last point by.
[[nodiscard]] std::int64_t packBlock(std::int32_t x, std::int32_t y, std::int32_t z) noexcept {
    const auto ux = static_cast<std::uint64_t>(static_cast<std::int64_t>(x)) & 0x3FFFFFFULL;
    const auto uy = static_cast<std::uint64_t>(static_cast<std::int64_t>(y)) & 0xFFFULL;
    const auto uz = static_cast<std::uint64_t>(static_cast<std::int64_t>(z)) & 0x3FFFFFFULL;
    return static_cast<std::int64_t>((ux << 38) | (uz << 12) | uy);
}

/// parity: `DfRewriteRule.SliceUniformAxes`. A subtree that ignores an axis its parent uses is
/// sampled at coordinate zero on that axis.
class SliceUniformAxes final : public RewriteRule {
public:
    explicit SliceUniformAxes(std::int32_t parentAxes) : parentAxes_(parentAxes) {}

    [[nodiscard]] Function rewrite(const Function& function) const override {
        if (function->kind() == Kind::Constant || function->kind() == Kind::Gradient) {
            return function;
        }
        const std::int32_t axes = function->domainAxes();
        if (axes == parentAxes_) {
            return function->rewriteChildren(function, *this);
        }
        Function rewritten = function->rewriteChildren(function, SliceUniformAxes{axes});
        return removeAxes(std::move(rewritten), parentAxes_ & ~axes);
    }

private:
    [[nodiscard]] static std::int32_t existingRemoved(const Function& function) {
        std::int32_t axes = 0;
        const DensityFunction* current = function.get();
        while (current->kind() == Kind::Slice) {
            const auto& slice = static_cast<const nodes::Slice&>(*current);
            axes |= axesFrom(slice.axis());
            current = slice.input().get();
        }
        return axes;
    }

    [[nodiscard]] static Function removeAxes(Function function, std::int32_t axes) {
        const std::int32_t filtered = axes & ~existingRemoved(function);
        if ((filtered & kAxisX) != 0) {
            function = functions::slice(Axis::X, 0, std::move(function));
        }
        if ((filtered & kAxisZ) != 0) {
            function = functions::slice(Axis::Z, 0, std::move(function));
        }
        if ((filtered & kAxisY) != 0) {
            function = functions::slice(Axis::Y, 0, std::move(function));
        }
        return function;
    }

    std::int32_t parentAxes_;
};

} // namespace

/// parity: the compiler's own rule: inline references and swap each cache for its prepared one.
class DensityCompiler::InlineRule final : public RewriteRule {
public:
    explicit InlineRule(DensityCompiler& compiler) : compiler_(&compiler) {}

    [[nodiscard]] Function rewrite(const Function& function) const override {
        const Function& inlined = inlineReference(function);
        if (inlined->kind() == Kind::Cache) {
            return compiler_->prepareCache(inlined);
        }
        return inlined->rewriteChildren(inlined, *this);
    }

private:
    DensityCompiler* compiler_;
};

ScopedBuffer::ScopedBuffer(SamplerContext& owner, std::int32_t size)
    : owner_(&owner), storage_(owner.take(size)), view_(std::span<float>{storage_}) {}

ScopedBuffer::~ScopedBuffer() {
    if (owner_ != nullptr) {
        owner_->give(std::move(storage_));
    }
}

ScopedBuffer::ScopedBuffer(ScopedBuffer&& other) noexcept
    : owner_(std::exchange(other.owner_, nullptr)), storage_(std::move(other.storage_)),
      view_(std::span<float>{storage_}) {}

SamplerContext::SamplerContext(Options options) : options_(std::move(options)) {}

std::vector<float> SamplerContext::take(std::int32_t size) {
    std::vector<float> storage;
    if (!pool_.empty()) {
        storage = std::move(pool_.back());
        pool_.pop_back();
    }
    storage.resize(static_cast<std::size_t>(size));
    return storage;
}

void SamplerContext::give(std::vector<float> storage) {
    if (pool_.size() < 64) {
        pool_.push_back(std::move(storage));
    }
}

void SamplerContext::sampleVolumeCached(std::int32_t cacheId, const DensitySampler& input, DensityBuffer& output,
                                        const DensityVolume& volume) {
    if (!options_.enableCaches) {
        input.sampleVolume(*this, output, volume);
        return;
    }
    if (cells_.size() <= static_cast<std::size_t>(cacheId)) {
        cells_.resize(static_cast<std::size_t>(cacheId) + 1);
    }
    if (cells_[static_cast<std::size_t>(cacheId)] == nullptr) {
        cells_[static_cast<std::size_t>(cacheId)] = std::make_unique<CacheCell>();
    }
    CacheCell& cell = *cells_[static_cast<std::size_t>(cacheId)];
    if (!cell.volume.has_value() || *cell.volume != volume) {
        cell.volume = volume;
        cell.buffer.assign(static_cast<std::size_t>(volume.size()), 0.0F);
        DensityBuffer view{std::span<float>{cell.buffer}};
        input.sampleVolume(*this, view, volume);
    }
    std::ranges::copy(cell.buffer, output.values().begin());
}

float SamplerContext::sampleValueCached(std::int32_t cacheId, const DensitySampler& input, std::int32_t x,
                                        std::int32_t y, std::int32_t z) {
    if (!options_.enableCaches) {
        return input.sampleValue(*this, x, y, z);
    }
    if (cells_.size() <= static_cast<std::size_t>(cacheId)) {
        cells_.resize(static_cast<std::size_t>(cacheId) + 1);
    }
    if (cells_[static_cast<std::size_t>(cacheId)] == nullptr) {
        cells_[static_cast<std::size_t>(cacheId)] = std::make_unique<CacheCell>();
    }
    CacheCell& cell = *cells_[static_cast<std::size_t>(cacheId)];
    const std::int64_t key = packBlock(x, y, z);
    if (cell.hasValue && cell.valueKey == key && !std::isnan(cell.value)) {
        return cell.value;
    }
    if (cell.volume.has_value()) {
        const std::int32_t index = cell.volume->indexOfBlock(x, y, z);
        if (index != DensityVolume::kNoBlock) {
            return cell.buffer[static_cast<std::size_t>(index)];
        }
    }
    const float value = input.sampleValue(*this, x, y, z);
    cell.valueKey = key;
    cell.value = value;
    cell.hasValue = true;
    return value;
}

DensityCompiler::DensityCompiler(std::unique_ptr<CompileContext> context) : context_(std::move(context)) {}

DensityCompiler::~DensityCompiler() = default;

Function DensityCompiler::optimize(const Function& function) {
    const InlineRule inlineRule{*this};
    const SliceUniformAxes slice{kAllAxes};
    return slice.rewrite(inlineRule.rewrite(function));
}

Function DensityCompiler::prepareCache(const Function& cache) {
    const Function& input = static_cast<const nodes::Cache&>(*cache).input();
    if (const auto found = preparedCaches_.find(input); found != preparedCaches_.end()) {
        return found->second;
    }
    const std::int32_t id = nextCacheId_++;
    const Function optimized = optimize(input);
    Sampler caching = nodes::makeCachingSampler(id, optimized->compile(*context_));
    Function prepared = std::make_shared<const nodes::PreparedCache>(id, optimized->range(), optimized->domainAxes(),
                                                                     std::move(caching));
    preparedCaches_.emplace(input, prepared);
    return prepared;
}

Sampler DensityCompiler::sampler(const Function& function) {
    const std::lock_guard<std::recursive_mutex> guard{mutex_};
    if (const auto found = samplers_.find(function); found != samplers_.end()) {
        return found->second;
    }
    Sampler compiled = optimize(function)->compile(*context_);
    samplers_.emplace(function, compiled);
    return compiled;
}

float sampleUncached(const DensitySampler& sampler, std::int32_t x, std::int32_t y, std::int32_t z) {
    SamplerContext context{SamplerContext::Options{}};
    return sampler.sampleValue(context, x, y, z);
}

} // namespace basalt::server::world::densityfunction
