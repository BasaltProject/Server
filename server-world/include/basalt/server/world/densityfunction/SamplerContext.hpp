#pragma once

#include <basalt/server/world/densityfunction/DensityFunction.hpp>
#include <basalt/server/world/densityfunction/DensityVolume.hpp>

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

namespace basalt::server::world::densityfunction {

class SamplerContext;

/// A pooled float buffer, handed back to its context when it goes out of scope.
class ScopedBuffer {
public:
    ScopedBuffer(SamplerContext& owner, std::int32_t size);
    ~ScopedBuffer();
    ScopedBuffer(const ScopedBuffer&) = delete;
    ScopedBuffer& operator=(const ScopedBuffer&) = delete;
    ScopedBuffer(ScopedBuffer&& other) noexcept;
    ScopedBuffer& operator=(ScopedBuffer&&) = delete;

    [[nodiscard]] DensityBuffer& buffer() noexcept { return view_; }

    [[nodiscard]] float get(std::int32_t index) const noexcept { return view_.get(index); }

private:
    SamplerContext* owner_;
    std::vector<float> storage_;
    DensityBuffer view_;
};

/// parity: `SamplerContext`. Per-use state for compiled samplers: the cache cells, the user fields
/// (the beardifier), and a buffer pool. One per chunk and thread, never shared.
class SamplerContext {
public:
    struct Options {
        bool enableCaches = false;
        /// The `beardifier` user field. Null samples it as its fallback, zero.
        Sampler beardifier;
    };

    explicit SamplerContext(Options options);

    [[nodiscard]] ScopedBuffer acquire(std::int32_t size) { return ScopedBuffer{*this, size}; }

    [[nodiscard]] const DensitySampler* beardifier() const noexcept { return options_.beardifier.get(); }

    /// parity: `SamplerContext.sampleVolumeCached`.
    void sampleVolumeCached(std::int32_t cacheId, const DensitySampler& input, DensityBuffer& output,
                            const DensityVolume& volume);

    /// parity: `SamplerContext.sampleValueCached`. A cell whose volume covers the block answers
    /// from that buffer, which can differ in the last bit from sampling the point.
    [[nodiscard]] float sampleValueCached(std::int32_t cacheId, const DensitySampler& input, std::int32_t x,
                                          std::int32_t y, std::int32_t z);

private:
    friend class ScopedBuffer;

    struct CacheCell {
        std::optional<DensityVolume> volume;
        std::vector<float> buffer;
        std::int64_t valueKey = 0;
        float value = 0.0F;
        bool hasValue = false;
    };

    [[nodiscard]] std::vector<float> take(std::int32_t size);
    void give(std::vector<float> storage);

    Options options_;
    std::vector<std::unique_ptr<CacheCell>> cells_;
    std::vector<std::vector<float>> pool_;
};

/// parity: `DensityFunctionCompiler`, which inlines references, deduplicates caches by structural
/// equality of their inputs, slices away the axes a subtree ignores, and compiles.
class DensityCompiler {
public:
    explicit DensityCompiler(std::unique_ptr<CompileContext> context);
    ~DensityCompiler();
    DensityCompiler(const DensityCompiler&) = delete;
    DensityCompiler& operator=(const DensityCompiler&) = delete;
    DensityCompiler(DensityCompiler&&) = delete;
    DensityCompiler& operator=(DensityCompiler&&) = delete;

    /// Safe from any thread: compiled samplers are immutable and shared.
    [[nodiscard]] Sampler sampler(const Function& function);

    [[nodiscard]] CompileContext& context() noexcept { return *context_; }

    /// The rewrite the compiler applies before compiling, exposed for the prepared caches.
    [[nodiscard]] Function optimize(const Function& function);

private:
    struct Hash {
        std::size_t operator()(const Function& function) const { return function->hash(); }
    };

    struct Equal {
        bool operator()(const Function& left, const Function& right) const { return equal(left, right); }
    };

    [[nodiscard]] Function prepareCache(const Function& cache);

    class InlineRule;

    std::unique_ptr<CompileContext> context_;
    std::recursive_mutex mutex_;
    std::unordered_map<Function, Sampler, Hash, Equal> samplers_;
    std::unordered_map<Function, Function, Hash, Equal> preparedCaches_;
    std::int32_t nextCacheId_ = 0;
};

/// Samples a function at one block with no cache, the way vanilla's debug path does.
[[nodiscard]] float sampleUncached(const DensitySampler& sampler, std::int32_t x, std::int32_t y, std::int32_t z);

} // namespace basalt::server::world::densityfunction
