#include "Nodes.hpp"

#include <basalt/server/world/noise/JavaMath.hpp>

#include <cmath>
#include <map>
#include <stdexcept>

namespace basalt::server::world::densityfunction::nodes {
namespace {

namespace jm = noise::javaMath;

/// parity: `CachingDensitySampler`.
class CachingSampler final : public DensitySampler {
public:
    CachingSampler(std::int32_t id, Sampler input) : id_(id), input_(std::move(input)) {}

    [[nodiscard]] float sampleValue(SamplerContext& context, std::int32_t x, std::int32_t y,
                                    std::int32_t z) const override {
        return context.sampleValueCached(id_, *input_, x, y, z);
    }

    void sampleVolume(SamplerContext& context, DensityBuffer& output, const DensityVolume& volume) const override {
        context.sampleVolumeCached(id_, *input_, output, volume);
    }

private:
    std::int32_t id_;
    Sampler input_;
};

/// parity: `InterpolatedFunction.Sampler`.
class InterpolatedSampler final : public DensitySampler {
public:
    InterpolatedSampler(Sampler input, std::int32_t cellXz, std::int32_t cellY)
        : input_(std::move(input)), cellXz_(cellXz), cellY_(cellY), cellXzInv_(1.0F / static_cast<float>(cellXz)),
          cellYInv_(1.0F / static_cast<float>(cellY)) {}

    [[nodiscard]] float sampleValue(SamplerContext& context, std::int32_t x, std::int32_t y,
                                    std::int32_t z) const override {
        const std::int32_t inX = jm::floorMod(x, cellXz_);
        const std::int32_t inY = jm::floorMod(y, cellY_);
        const std::int32_t inZ = jm::floorMod(z, cellXz_);
        if (inX == 0 && inY == 0 && inZ == 0) {
            return input_->sampleValue(context, x, y, z);
        }
        const DensityVolume volume{2, 2, 2, x - inX, y - inY, z - inZ, cellXz_, cellY_, cellXz_};
        ScopedBuffer buffer = context.acquire(volume.size());
        input_->sampleVolume(context, buffer.buffer(), volume);
        return jm::lerp3(static_cast<float>(inX) / static_cast<float>(cellXz_),
                         static_cast<float>(inY) / static_cast<float>(cellY_),
                         static_cast<float>(inZ) / static_cast<float>(cellXz_),
                         buffer.get(volume.indexUnchecked(0, 0, 0)), buffer.get(volume.indexUnchecked(1, 0, 0)),
                         buffer.get(volume.indexUnchecked(0, 1, 0)), buffer.get(volume.indexUnchecked(1, 1, 0)),
                         buffer.get(volume.indexUnchecked(0, 0, 1)), buffer.get(volume.indexUnchecked(1, 0, 1)),
                         buffer.get(volume.indexUnchecked(0, 1, 1)), buffer.get(volume.indexUnchecked(1, 1, 1)));
    }

    void sampleVolume(SamplerContext& context, DensityBuffer& output, const DensityVolume& volume) const override {
        if ((volume.stepBlockX == cellXz_ || volume.sizeX == 1) && (volume.stepBlockY == cellY_ || volume.sizeY == 1) &&
            (volume.stepBlockZ == cellXz_ || volume.sizeZ == 1) && jm::floorMod(volume.minBlockX, cellXz_) == 0 &&
            jm::floorMod(volume.minBlockY, cellY_) == 0 && jm::floorMod(volume.minBlockZ, cellXz_) == 0) {
            input_->sampleVolume(context, output, volume);
        } else if (volume.stepBlockX == 1 && volume.stepBlockY == 1 && volume.stepBlockZ == 1) {
            sampleWithBlockStep(context, output, volume);
        } else {
            const DensityVolume blocks{volume.sizeX * volume.stepBlockX,
                                       volume.sizeY * volume.stepBlockY,
                                       volume.sizeZ * volume.stepBlockZ,
                                       volume.minBlockX,
                                       volume.minBlockY,
                                       volume.minBlockZ,
                                       1,
                                       1,
                                       1};
            ScopedBuffer buffer = context.acquire(blocks.size());
            sampleWithBlockStep(context, buffer.buffer(), blocks);
            for (std::int32_t z = 0; z < volume.sizeZ; ++z) {
                for (std::int32_t x = 0; x < volume.sizeX; ++x) {
                    for (std::int32_t y = 0; y < volume.sizeY; ++y) {
                        output.set(volume.indexUnchecked(x, y, z),
                                   buffer.get(blocks.indexUnchecked(x * volume.stepBlockX, y * volume.stepBlockY,
                                                                    z * volume.stepBlockZ)));
                    }
                }
            }
        }
    }

private:
    void sampleWithBlockStep(SamplerContext& context, DensityBuffer& output, const DensityVolume& volume) const {
        const std::int32_t minCellX = jm::floorDiv(volume.minBlockX, cellXz_);
        const std::int32_t minCellY = jm::floorDiv(volume.minBlockY, cellY_);
        const std::int32_t minCellZ = jm::floorDiv(volume.minBlockZ, cellXz_);
        const std::int32_t countX = jm::floorDiv(volume.maxBlockX(), cellXz_) - minCellX + 1;
        const std::int32_t countY = jm::floorDiv(volume.maxBlockY(), cellY_) - minCellY + 1;
        const std::int32_t countZ = jm::floorDiv(volume.maxBlockZ(), cellXz_) - minCellZ + 1;
        const DensityVolume cells{jm::floorMod(volume.maxBlockX(), cellXz_) == 0 ? countX : countX + 1,
                                  jm::floorMod(volume.maxBlockY(), cellY_) == 0 ? countY : countY + 1,
                                  jm::floorMod(volume.maxBlockZ(), cellXz_) == 0 ? countZ : countZ + 1,
                                  minCellX * cellXz_,
                                  minCellY * cellY_,
                                  minCellZ * cellXz_,
                                  cellXz_,
                                  cellY_,
                                  cellXz_};
        ScopedBuffer cellScope = context.acquire(cells.size());
        input_->sampleVolume(context, cellScope.buffer(), cells);
        const DensityBuffer& cell = cellScope.buffer();
        for (std::int32_t cellZ = 0; cellZ < countZ; ++cellZ) {
            const std::int32_t nextZ = std::min(cellZ + 1, cells.sizeZ - 1);
            for (std::int32_t cellX = 0; cellX < countX; ++cellX) {
                const std::int32_t nextX = std::min(cellX + 1, cells.sizeX - 1);
                float v000 = cell.get(cells.indexUnchecked(cellX, 0, cellZ));
                float v100 = cell.get(cells.indexUnchecked(nextX, 0, cellZ));
                float v001 = cell.get(cells.indexUnchecked(cellX, 0, nextZ));
                float v101 = cell.get(cells.indexUnchecked(nextX, 0, nextZ));
                for (std::int32_t cellY = 0; cellY < countY; ++cellY) {
                    const std::int32_t nextY = std::min(cellY + 1, cells.sizeY - 1);
                    const float v010 = cell.get(cells.indexUnchecked(cellX, nextY, cellZ));
                    const float v110 = cell.get(cells.indexUnchecked(nextX, nextY, cellZ));
                    const float v011 = cell.get(cells.indexUnchecked(cellX, nextY, nextZ));
                    const float v111 = cell.get(cells.indexUnchecked(nextX, nextY, nextZ));
                    fillCell(output, volume, cells, cellX, cellY, cellZ,
                             {v000, v100, v010, v110, v001, v101, v011, v111});
                    v000 = v010;
                    v100 = v110;
                    v001 = v011;
                    v101 = v111;
                }
            }
        }
    }

    void fillCell(DensityBuffer& output, const DensityVolume& volume, const DensityVolume& cells, std::int32_t cellX,
                  std::int32_t cellY, std::int32_t cellZ, const std::array<float, 8>& v) const {
        const std::int32_t outX = cells.blockX(cellX) - volume.minBlockX;
        const std::int32_t outY = cells.blockY(cellY) - volume.minBlockY;
        const std::int32_t outZ = cells.blockZ(cellZ) - volume.minBlockZ;
        const std::int32_t x0 = std::max(0, -outX);
        const std::int32_t y0 = std::max(0, -outY);
        const std::int32_t z0 = std::max(0, -outZ);
        const std::int32_t x1 = std::min(cellXz_, volume.sizeX - outX) - 1;
        const std::int32_t y1 = std::min(cellY_, volume.sizeY - outY) - 1;
        const std::int32_t z1 = std::min(cellXz_, volume.sizeZ - outZ) - 1;
        for (std::int32_t z = z0; z <= z1; ++z) {
            const float alphaZ = static_cast<float>(z) * cellXzInv_;
            const float v00 = jm::lerp(alphaZ, v[0], v[4]);
            const float v01 = jm::lerp(alphaZ, v[2], v[6]);
            const float v10 = jm::lerp(alphaZ, v[1], v[5]);
            const float v11 = jm::lerp(alphaZ, v[3], v[7]);
            for (std::int32_t x = x0; x <= x1; ++x) {
                const float alphaX = static_cast<float>(x) * cellXzInv_;
                const float bottom = jm::lerp(alphaX, v00, v10);
                const float top = jm::lerp(alphaX, v01, v11);
                const float step = (top - bottom) * cellYInv_;
                float value = bottom + step * static_cast<float>(y0);
                std::int32_t index = volume.indexUnchecked(outX + x, outY + y0, outZ + z);
                for (std::int32_t y = y0; y <= y1; ++y) {
                    output.set(index++, value);
                    value += step;
                }
            }
        }
    }

    Sampler input_;
    std::int32_t cellXz_;
    std::int32_t cellY_;
    float cellXzInv_;
    float cellYInv_;
};

/// parity: `SliceFunction`'s X, Y, Z and XZ samplers in one: `-1` means "not sliced".
class SliceSampler final : public DensitySampler {
public:
    SliceSampler(Sampler input, std::optional<std::int32_t> x, std::optional<std::int32_t> y,
                 std::optional<std::int32_t> z)
        : input_(std::move(input)), x_(x), y_(y), z_(z) {}

    [[nodiscard]] float sampleValue(SamplerContext& context, std::int32_t x, std::int32_t y,
                                    std::int32_t z) const override {
        return input_->sampleValue(context, x_.value_or(x), y_.value_or(y), z_.value_or(z));
    }

    void sampleVolume(SamplerContext& context, DensityBuffer& output, const DensityVolume& volume) const override {
        if ((!x_.has_value() || (volume.sizeX == 1 && volume.minBlockX == *x_)) &&
            (!y_.has_value() || (volume.sizeY == 1 && volume.minBlockY == *y_)) &&
            (!z_.has_value() || (volume.sizeZ == 1 && volume.minBlockZ == *z_))) {
            input_->sampleVolume(context, output, volume);
            return;
        }
        DensityVolume input = volume;
        if (x_.has_value()) {
            input.sizeX = 1;
            input.minBlockX = *x_;
        }
        if (y_.has_value()) {
            input.sizeY = 1;
            input.minBlockY = *y_;
        }
        if (z_.has_value()) {
            input.sizeZ = 1;
            input.minBlockZ = *z_;
        }
        ScopedBuffer buffer = context.acquire(input.size());
        input_->sampleVolume(context, buffer.buffer(), input);
        std::int32_t index = 0;
        for (std::int32_t z = 0; z < volume.sizeZ; ++z) {
            for (std::int32_t x = 0; x < volume.sizeX; ++x) {
                for (std::int32_t y = 0; y < volume.sizeY; ++y) {
                    output.set(index++, buffer.get(input.indexUnchecked(x_.has_value() ? 0 : x, y_.has_value() ? 0 : y,
                                                                        z_.has_value() ? 0 : z)));
                }
            }
        }
    }

private:
    Sampler input_;
    std::optional<std::int32_t> x_;
    std::optional<std::int32_t> y_;
    std::optional<std::int32_t> z_;
};

/// parity: `FindTopSurfaceFunction.Sampler`.
class FindTopSurfaceSampler final : public DensitySampler {
public:
    FindTopSurfaceSampler(Sampler density, Sampler upperBound, std::int32_t lowerBound, std::int32_t cellHeight)
        : density_(std::move(density)), upperBound_(std::move(upperBound)), lowerBound_(lowerBound),
          cellHeight_(cellHeight) {}

    [[nodiscard]] float sampleValue(SamplerContext& context, std::int32_t x, std::int32_t y,
                                    std::int32_t z) const override {
        return static_cast<float>(findSurface(context, x, z, upperBound_->sampleValue(context, x, y, z)));
    }

    void sampleVolume(SamplerContext& context, DensityBuffer& output, const DensityVolume& volume) const override {
        if (volume.sizeY != 1) {
            throw std::invalid_argument{"find_top_surface sampled with a height other than one"};
        }
        upperBound_->sampleVolume(context, output, volume);
        std::int32_t index = 0;
        for (std::int32_t z = 0; z < volume.sizeZ; ++z) {
            const std::int32_t blockZ = volume.blockZ(z);
            for (std::int32_t x = 0; x < volume.sizeX; ++x) {
                const float upper = output.get(index);
                output.set(index, static_cast<float>(findSurface(context, volume.blockX(x), blockZ, upper)));
                ++index;
            }
        }
    }

private:
    [[nodiscard]] std::int32_t findSurface(SamplerContext& context, std::int32_t x, std::int32_t z,
                                           float upperBound) const {
        const std::int32_t topY = jm::floor(upperBound / static_cast<float>(cellHeight_)) * cellHeight_;
        if (topY <= lowerBound_) {
            return lowerBound_;
        }
        for (std::int32_t probeY = topY; probeY >= lowerBound_; probeY -= cellHeight_) {
            if (density_->sampleValue(context, x, probeY, z) > 0.0F) {
                return probeY;
            }
        }
        return lowerBound_;
    }

    Sampler density_;
    Sampler upperBound_;
    std::int32_t lowerBound_;
    std::int32_t cellHeight_;
};

struct CompiledSpline {
    std::optional<float> constant;
    std::int32_t coordinate = 0;
    std::vector<float> locations;
    std::vector<CompiledSpline> values;
    std::vector<float> derivatives;
};

[[nodiscard]] float linearExtend(float input, const std::vector<float>& locations, float value,
                                 const std::vector<float>& derivatives, std::size_t index) {
    const float derivative = derivatives[index];
    return derivative == 0.0F ? value : value + derivative * (input - locations[index]);
}

/// parity: `SplineFunction`'s point and buffer inputs.
class SplineInput {
public:
    virtual ~SplineInput() = default;
    [[nodiscard]] virtual float coordinate(std::int32_t index) = 0;
};

[[nodiscard]] float sampleSpline(const CompiledSpline& spline, SplineInput& input) {
    if (spline.constant.has_value()) {
        return *spline.constant;
    }
    const float value = input.coordinate(spline.coordinate);
    // parity: `Mth.binarySearch(0, n, i -> input < locations[i]) - 1`.
    std::size_t low = 0;
    std::size_t high = spline.locations.size();
    while (low < high) {
        const std::size_t mid = low + (high - low) / 2;
        if (value < spline.locations[mid]) {
            high = mid;
        } else {
            low = mid + 1;
        }
    }
    const std::int64_t start = static_cast<std::int64_t>(low) - 1;
    const std::size_t last = spline.locations.size() - 1;
    if (start < 0) {
        return linearExtend(value, spline.locations, sampleSpline(spline.values.front(), input), spline.derivatives, 0);
    }
    const auto begin = static_cast<std::size_t>(start);
    if (begin == last) {
        return linearExtend(value, spline.locations, sampleSpline(spline.values[last], input), spline.derivatives,
                            last);
    }
    const float x1 = spline.locations[begin];
    const float x2 = spline.locations[begin + 1];
    const float t = (value - x1) / (x2 - x1);
    const float d1 = spline.derivatives[begin];
    const float d2 = spline.derivatives[begin + 1];
    const float y1 = sampleSpline(spline.values[begin], input);
    const float y2 = sampleSpline(spline.values[begin + 1], input);
    const float a = d1 * (x2 - x1) - (y2 - y1);
    const float b = -d2 * (x2 - x1) + (y2 - y1);
    return jm::lerp(t, y1, y2) + t * (1.0F - t) * jm::lerp(t, a, b);
}

class SplineSampler final : public DensitySampler {
public:
    SplineSampler(CompiledSpline spline, std::vector<Sampler> coordinates)
        : spline_(std::move(spline)), coordinates_(std::move(coordinates)) {}

    [[nodiscard]] float sampleValue(SamplerContext& context, std::int32_t x, std::int32_t y,
                                    std::int32_t z) const override {
        PointInput input{*this, context, x, y, z};
        return sampleSpline(spline_, input);
    }

    void sampleVolume(SamplerContext& context, DensityBuffer& output, const DensityVolume& volume) const override {
        BufferInput input{*this, context, volume};
        for (std::int32_t i = 0; i < output.size(); ++i) {
            input.index = i;
            output.set(i, sampleSpline(spline_, input));
        }
    }

private:
    struct PointInput final : SplineInput {
        PointInput(const SplineSampler& owner, SamplerContext& context, std::int32_t x, std::int32_t y, std::int32_t z)
            : owner(owner), context(context), x(x), y(y), z(z), cached(owner.coordinates_.size()) {}

        float coordinate(std::int32_t index) override {
            std::optional<float>& slot = cached[static_cast<std::size_t>(index)];
            if (!slot.has_value()) {
                slot = owner.coordinates_[static_cast<std::size_t>(index)]->sampleValue(context, x, y, z);
            }
            return *slot;
        }

        const SplineSampler& owner;
        SamplerContext& context;
        std::int32_t x;
        std::int32_t y;
        std::int32_t z;
        std::vector<std::optional<float>> cached;
    };

    struct BufferInput final : SplineInput {
        BufferInput(const SplineSampler& owner, SamplerContext& context, const DensityVolume& volume)
            : owner(owner), context(context), volume(volume), buffers(owner.coordinates_.size()) {}

        float coordinate(std::int32_t coordinateIndex) override {
            std::optional<ScopedBuffer>& slot = buffers[static_cast<std::size_t>(coordinateIndex)];
            if (!slot.has_value()) {
                slot.emplace(context.acquire(volume.size()));
                owner.coordinates_[static_cast<std::size_t>(coordinateIndex)]->sampleVolume(context, slot->buffer(),
                                                                                            volume);
            }
            return slot->get(index);
        }

        const SplineSampler& owner;
        SamplerContext& context;
        const DensityVolume& volume;
        std::vector<std::optional<ScopedBuffer>> buffers;
        std::int32_t index = 0;
    };

    CompiledSpline spline_;
    std::vector<Sampler> coordinates_;
};

using CoordinateMap = std::vector<std::pair<Function, Sampler>>;

[[nodiscard]] CompiledSpline compileSpline(const Spline& spline, CompileContext& context, CoordinateMap& coordinates) {
    CompiledSpline compiled;
    if (spline.constant.has_value()) {
        compiled.constant = spline.constant;
        return compiled;
    }
    // parity: the coordinate map is a `HashMap` keyed by function equality.
    std::int32_t index = -1;
    for (std::size_t i = 0; i < coordinates.size(); ++i) {
        if (equal(coordinates[i].first, spline.coordinate)) {
            index = static_cast<std::int32_t>(i);
            break;
        }
    }
    if (index < 0) {
        index = static_cast<std::int32_t>(coordinates.size());
        coordinates.emplace_back(spline.coordinate, spline.coordinate->compile(context));
    }
    compiled.coordinate = index;
    for (const Spline::Point& point : spline.points) {
        compiled.locations.push_back(point.location);
        compiled.derivatives.push_back(point.derivative);
        compiled.values.push_back(compileSpline(*point.value, context, coordinates));
    }
    return compiled;
}

[[nodiscard]] Interval splineRange(const Spline& spline) {
    if (spline.constant.has_value()) {
        return Interval::ofExact(*spline.constant);
    }
    const Interval input = spline.coordinate->range();
    if (input.isNaI()) {
        return input;
    }
    std::vector<float> locations;
    std::vector<float> derivatives;
    std::vector<Interval> ranges;
    for (const Spline::Point& point : spline.points) {
        locations.push_back(point.location);
        derivatives.push_back(point.derivative);
        ranges.push_back(splineRange(*point.value));
    }
    const std::size_t last = locations.size() - 1;
    float minValue = std::numeric_limits<float>::infinity();
    float maxValue = -std::numeric_limits<float>::infinity();
    if (input.min() < locations[0]) {
        const float edge1 = linearExtend(input.min(), locations, ranges.front().min(), derivatives, 0);
        const float edge2 = linearExtend(input.min(), locations, ranges.front().max(), derivatives, 0);
        minValue = jm::fmin(minValue, jm::fmin(edge1, edge2));
        maxValue = jm::fmax(maxValue, jm::fmax(edge1, edge2));
    }
    if (input.max() > locations[last]) {
        const float edge1 = linearExtend(input.max(), locations, ranges[last].min(), derivatives, last);
        const float edge2 = linearExtend(input.max(), locations, ranges[last].max(), derivatives, last);
        minValue = jm::fmin(minValue, jm::fmin(edge1, edge2));
        maxValue = jm::fmax(maxValue, jm::fmax(edge1, edge2));
    }
    for (const Interval& range : ranges) {
        minValue = jm::fmin(minValue, range.min());
        maxValue = jm::fmax(maxValue, range.max());
    }
    for (std::size_t i = 0; i < last; ++i) {
        const float xDiff = locations[i + 1] - locations[i];
        const float min1 = ranges[i].min();
        const float max1 = ranges[i].max();
        const float min2 = ranges[i + 1].min();
        const float max2 = ranges[i + 1].max();
        const float d1 = derivatives[i];
        const float d2 = derivatives[i + 1];
        if (d1 != 0.0F || d2 != 0.0F) {
            const float p1 = d1 * xDiff;
            const float p2 = d2 * xDiff;
            const float minLerp1 = jm::fmin(min1, min2);
            const float maxLerp1 = jm::fmax(max1, max2);
            const float minA = p1 - max2 + min1;
            const float maxA = p1 - min2 + max1;
            const float minB = -p2 + min2 - max1;
            const float maxB = -p2 + max2 - min1;
            minValue = jm::fmin(minValue, minLerp1 + 0.25F * jm::fmin(minA, minB));
            maxValue = jm::fmax(maxValue, maxLerp1 + 0.25F * jm::fmax(maxA, maxB));
        }
    }
    return Interval::of(minValue, maxValue);
}

void forEachCoordinate(const Spline& spline, const std::function<void(const Function&)>& visit) {
    if (spline.constant.has_value()) {
        return;
    }
    visit(spline.coordinate);
    for (const Spline::Point& point : spline.points) {
        forEachCoordinate(*point.value, visit);
    }
}

[[nodiscard]] std::shared_ptr<const Spline> rewriteSpline(const std::shared_ptr<const Spline>& spline,
                                                          const RewriteRule& rule, bool& changed) {
    if (spline->constant.has_value()) {
        return spline;
    }
    auto copy = std::make_shared<Spline>();
    copy->coordinate = rewriteChild(spline->coordinate, rule, changed);
    for (const Spline::Point& point : spline->points) {
        copy->points.push_back({point.location, rewriteSpline(point.value, rule, changed), point.derivative});
    }
    return copy;
}

[[nodiscard]] bool equalSpline(const Spline& a, const Spline& b) {
    if (a.constant.has_value() || b.constant.has_value()) {
        return a.constant.has_value() && b.constant.has_value() && sameFloat(*a.constant, *b.constant);
    }
    if (!equal(a.coordinate, b.coordinate) || a.points.size() != b.points.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.points.size(); ++i) {
        if (!sameFloat(a.points[i].location, b.points[i].location) ||
            !sameFloat(a.points[i].derivative, b.points[i].derivative) ||
            !equalSpline(*a.points[i].value, *b.points[i].value)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] std::size_t hashSpline(const Spline& spline) {
    if (spline.constant.has_value()) {
        return hashFloat(*spline.constant);
    }
    std::size_t seed = spline.coordinate->hash();
    for (const Spline::Point& point : spline.points) {
        seed = mix(mix(mix(seed, hashFloat(point.location)), hashFloat(point.derivative)), hashSpline(*point.value));
    }
    return seed;
}

} // namespace

Sampler ySliceSampler(Sampler input, std::int32_t y) {
    return std::make_shared<const SliceSampler>(std::move(input), std::nullopt, y, std::nullopt);
}

Function Reference::rewriteChildren(const Function& self, const RewriteRule& rule) const {
    Function rewritten = rule.rewrite(target_);
    return rewritten.get() == target_.get() ? self : rewritten;
}

bool Reference::equals(const DensityFunction& other) const {
    return other.kind() == Kind::Reference && static_cast<const Reference&>(other).id_ == id_;
}

Sampler Cache::compile(CompileContext&) const {
    throw std::logic_error{"a cache is compiled only after the compiler has deduplicated it"};
}

Function Cache::rewriteChildren(const Function& self, const RewriteRule& rule) const {
    bool changed = false;
    Function input = rewriteChild(input_, rule, changed);
    return changed ? std::make_shared<const Cache>(input) : self;
}

bool Cache::equals(const DensityFunction& other) const {
    return other.kind() == Kind::Cache && equal(input_, static_cast<const Cache&>(other).input_);
}

bool PreparedCache::equals(const DensityFunction& other) const {
    return other.kind() == Kind::PreparedCache && static_cast<const PreparedCache&>(other).id_ == id_;
}

Sampler makeCachingSampler(std::int32_t id, Sampler input) {
    return std::make_shared<const CachingSampler>(id, std::move(input));
}

Function BlendDensity::rewriteChildren(const Function& self, const RewriteRule& rule) const {
    bool changed = false;
    Function input = rewriteChild(input_, rule, changed);
    return changed ? std::make_shared<const BlendDensity>(input) : self;
}

bool BlendDensity::equals(const DensityFunction& other) const {
    return other.kind() == Kind::BlendDensity && equal(input_, static_cast<const BlendDensity&>(other).input_);
}

Sampler Interpolated::compile(CompileContext& context) const {
    return std::make_shared<const InterpolatedSampler>(input_->compile(context), cellXz_, cellY_);
}

Function Interpolated::rewriteChildren(const Function& self, const RewriteRule& rule) const {
    bool changed = false;
    Function input = rewriteChild(input_, rule, changed);
    return changed ? std::make_shared<const Interpolated>(input, cellXz_, cellY_) : self;
}

bool Interpolated::equals(const DensityFunction& other) const {
    if (other.kind() != Kind::Interpolated) {
        return false;
    }
    const auto& that = static_cast<const Interpolated&>(other);
    return cellXz_ == that.cellXz_ && cellY_ == that.cellY_ && equal(input_, that.input_);
}

std::size_t Interpolated::hash() const {
    return mix(mix(mix(19, input_->hash()), static_cast<std::size_t>(cellXz_)), static_cast<std::size_t>(cellY_));
}

Sampler Slice::compile(CompileContext& context) const {
    // parity: an x slice over a z slice, or the other way round, becomes one xz sampler.
    if (input_->kind() == Kind::Slice) {
        const auto& inner = static_cast<const Slice&>(*input_);
        if ((axis_ == Axis::X && inner.axis_ == Axis::Z) || (axis_ == Axis::Z && inner.axis_ == Axis::X)) {
            const std::int32_t x = axis_ == Axis::X ? coordinate_ : inner.coordinate_;
            const std::int32_t z = axis_ == Axis::X ? inner.coordinate_ : coordinate_;
            return std::make_shared<const SliceSampler>(inner.input_->compile(context), x, std::nullopt, z);
        }
    }
    Sampler input = input_->compile(context);
    switch (axis_) {
    case Axis::X:
        return std::make_shared<const SliceSampler>(std::move(input), coordinate_, std::nullopt, std::nullopt);
    case Axis::Y:
        return ySliceSampler(std::move(input), coordinate_);
    case Axis::Z:
        return std::make_shared<const SliceSampler>(std::move(input), std::nullopt, std::nullopt, coordinate_);
    }
    return input;
}

Function Slice::rewriteChildren(const Function& self, const RewriteRule& rule) const {
    bool changed = false;
    Function input = rewriteChild(input_, rule, changed);
    return changed ? std::make_shared<const Slice>(axis_, coordinate_, input) : self;
}

bool Slice::equals(const DensityFunction& other) const {
    if (other.kind() != Kind::Slice) {
        return false;
    }
    const auto& that = static_cast<const Slice&>(other);
    return axis_ == that.axis_ && coordinate_ == that.coordinate_ && equal(input_, that.input_);
}

std::size_t Slice::hash() const {
    return mix(mix(mix(20, static_cast<std::size_t>(axis_)), static_cast<std::size_t>(coordinate_)), input_->hash());
}

Interval FindTopSurface::range() const {
    const auto lower = static_cast<float>(lowerBound_);
    return Interval::of(lower, jm::fmax(lower, upperBound_->range().max()));
}

Sampler FindTopSurface::compile(CompileContext& context) const {
    auto sampler = std::make_shared<const FindTopSurfaceSampler>(
        density_->compile(context), upperBound_->compile(context), lowerBound_, cellHeight_);
    return ySliceSampler(std::move(sampler), 0);
}

Function FindTopSurface::rewriteChildren(const Function& self, const RewriteRule& rule) const {
    bool changed = false;
    Function density = rewriteChild(density_, rule, changed);
    Function upper = rewriteChild(upperBound_, rule, changed);
    return changed ? std::make_shared<const FindTopSurface>(density, upper, lowerBound_, cellHeight_) : self;
}

bool FindTopSurface::equals(const DensityFunction& other) const {
    if (other.kind() != Kind::FindTopSurface) {
        return false;
    }
    const auto& that = static_cast<const FindTopSurface&>(other);
    return lowerBound_ == that.lowerBound_ && cellHeight_ == that.cellHeight_ && equal(density_, that.density_) &&
           equal(upperBound_, that.upperBound_);
}

std::size_t FindTopSurface::hash() const {
    return mix(mix(mix(mix(21, density_->hash()), upperBound_->hash()), static_cast<std::size_t>(lowerBound_)),
               static_cast<std::size_t>(cellHeight_));
}

Interval SplineNode::range() const { return splineRange(*spline_); }

std::int32_t SplineNode::domainAxes() const {
    std::int32_t axes = 0;
    forEachCoordinate(*spline_, [&axes](const Function& coordinate) { axes |= coordinate->domainAxes(); });
    return axes;
}

Sampler SplineNode::compile(CompileContext& context) const {
    CoordinateMap coordinates;
    CompiledSpline compiled = compileSpline(*spline_, context, coordinates);
    if (compiled.constant.has_value()) {
        return constantSampler(*compiled.constant);
    }
    std::vector<Sampler> samplers;
    samplers.reserve(coordinates.size());
    for (auto& [function, sampler] : coordinates) {
        samplers.push_back(sampler);
    }
    return std::make_shared<const SplineSampler>(std::move(compiled), std::move(samplers));
}

Function SplineNode::rewriteChildren(const Function& self, const RewriteRule& rule) const {
    bool changed = false;
    std::shared_ptr<const Spline> rewritten = rewriteSpline(spline_, rule, changed);
    return changed ? std::make_shared<const SplineNode>(std::move(rewritten)) : self;
}

bool SplineNode::equals(const DensityFunction& other) const {
    return other.kind() == Kind::Spline && equalSpline(*spline_, *static_cast<const SplineNode&>(other).spline_);
}

std::size_t SplineNode::hash() const { return mix(22, hashSpline(*spline_)); }

} // namespace basalt::server::world::densityfunction::nodes

namespace basalt::server::world::densityfunction {

bool equal(const Function& left, const Function& right) {
    return left.get() == right.get() || (left != nullptr && right != nullptr && left->equals(*right));
}

const Function& inlineReference(const Function& function) {
    if (function->kind() == Kind::Reference) {
        return static_cast<const nodes::Reference&>(*function).target();
    }
    return function;
}

namespace functions {

Function zero() {
    static const Function kZero = constant(0.0F);
    return kZero;
}

Function constant(float value) { return std::make_shared<const nodes::Constant>(value); }

Function simple(SimpleType type) { return std::make_shared<const nodes::Simple>(type); }

Function reference(std::string id, Function target) {
    return std::make_shared<const nodes::Reference>(std::move(id), std::move(target));
}

Function noise(std::string noiseId, std::shared_ptr<const noise::NormalNoise> parameters, double xzScale, double yScale,
               Function shiftX, Function shiftY, Function shiftZ) {
    return std::make_shared<const nodes::NoiseNode>(std::move(noiseId), std::move(parameters), xzScale, yScale,
                                                    std::move(shiftX), std::move(shiftY), std::move(shiftZ));
}

Function endIslands() { return std::make_shared<const nodes::EndIslands>(); }

Function distanceToPoint(std::int32_t x, std::int32_t y, std::int32_t z, DistanceMetric metric) {
    return std::make_shared<const nodes::DistanceToPoint>(x, y, z, metric);
}

Function gradient(Axis axis, Tiling tiling, std::int32_t fromCoordinate, std::int32_t toCoordinate, float fromValue,
                  float toValue) {
    return std::make_shared<const nodes::Gradient>(axis, tiling, fromCoordinate, toCoordinate, fromValue, toValue);
}

Function shift(ShiftType type, std::string noiseId, std::shared_ptr<const noise::NormalNoise> parameters) {
    return std::make_shared<const nodes::Shift>(type, std::move(noiseId), std::move(parameters));
}

Function unary(UnaryType type, Function input) { return std::make_shared<const nodes::Unary>(type, std::move(input)); }

Function binary(BinaryType type, Function left, Function right) {
    return std::make_shared<const nodes::Binary>(type, std::move(left), std::move(right));
}

Function spline(std::shared_ptr<const Spline> spline) {
    return std::make_shared<const nodes::SplineNode>(std::move(spline));
}

Function lerp(Function alpha, Function first, Function second) {
    return std::make_shared<const nodes::Lerp>(std::move(alpha), std::move(first), std::move(second));
}

Function clamp(Function input, float min, float max) {
    return std::make_shared<const nodes::Clamp>(std::move(input), min, max);
}

Function rangeChoice(Function input, float minInclusive, float maxExclusive, Function whenInRange,
                     Function whenOutOfRange) {
    return std::make_shared<const nodes::RangeChoice>(std::move(input), minInclusive, maxExclusive,
                                                      std::move(whenInRange), std::move(whenOutOfRange));
}

Function intervalSelect(Function input, std::vector<float> thresholds, std::vector<Function> functions) {
    return std::make_shared<const nodes::IntervalSelect>(std::move(input), std::move(thresholds), std::move(functions));
}

Function cache(Function input) { return std::make_shared<const nodes::Cache>(std::move(input)); }

Function blendDensity(Function input) { return std::make_shared<const nodes::BlendDensity>(std::move(input)); }

Function interpolated(Function input, std::int32_t cellSizeXz, std::int32_t cellSizeY) {
    return std::make_shared<const nodes::Interpolated>(std::move(input), cellSizeXz, cellSizeY);
}

Function slice(Axis axis, std::int32_t coordinate, Function input) {
    return std::make_shared<const nodes::Slice>(axis, coordinate, std::move(input));
}

Function findTopSurface(Function density, Function upperBound, std::int32_t lowerBound, std::int32_t cellHeight) {
    return std::make_shared<const nodes::FindTopSurface>(std::move(density), std::move(upperBound), lowerBound,
                                                         cellHeight);
}

Function blendedNoise(double xzScale, double yScale, double xzFactor, double yFactor, double smearScaleMultiplier) {
    return std::make_shared<const nodes::BlendedNoiseNode>(xzScale, yScale, xzFactor, yFactor, smearScaleMultiplier);
}

Sampler compileBlendedNoise(random::RandomSource& random, double xzScale, double yScale, double xzFactor,
                            double yFactor, double smearScaleMultiplier) {
    return nodes::BlendedNoiseNode{xzScale, yScale, xzFactor, yFactor, smearScaleMultiplier}.compileWith(random);
}

} // namespace functions
} // namespace basalt::server::world::densityfunction
