// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;

import it.unimi.dsi.fastutil.floats.Float2FloatFunction;
import java.util.ArrayList;
import java.util.List;
import java.util.function.Consumer;
import java.util.function.Function;

public sealed interface CubicSpline<I extends BoundedFloatFunction<?>> {
    Interval range();

    <O extends BoundedFloatFunction<?>> CubicSpline<O> mapCoordinates(Function<I, O> mapper);

    void forEachCoordinate(Consumer<I> visitor);

    static <I extends BoundedFloatFunction<?>> CubicSpline<I> constant(float value) {
        return new Constant<>(value);
    }

    static <I extends BoundedFloatFunction<?>> Builder<I> builder(I coordinate) {
        return new Builder<>(coordinate, Float2FloatFunction.identity());
    }

    static <I extends BoundedFloatFunction<?>> Builder<I> builder(I coordinate, Float2FloatFunction valueTransformer) {
        return new Builder<>(coordinate, valueTransformer);
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    static <C> BoundedFloatFunction<C> asSampler(CubicSpline<? extends BoundedFloatFunction<C>> spline) {
        return new BoundedFloatFunction<C>() {
            public float apply(C point) {
                return evaluate((CubicSpline) spline, point);
            }

            public Interval range() {
                return spline.range();
            }
        };
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    private static float evaluate(CubicSpline spline, Object point) {
        if (spline instanceof Constant<?> fixed) {
            return fixed.value();
        }
        Multipoint<?> multi = (Multipoint<?>) spline;
        float at = ((BoundedFloatFunction) multi.coordinate()).apply(point);
        int start = multi.segment(at);
        int last = multi.locations().length - 1;
        if (start < 0) {
            return multi.extend(at, evaluate(multi.values().get(0), point), 0);
        }
        if (start == last) {
            return multi.extend(at, evaluate(multi.values().get(last), point), last);
        }
        float x0 = multi.locations()[start];
        float x1 = multi.locations()[start + 1];
        float t = (at - x0) / (x1 - x0);
        float y0 = evaluate(multi.values().get(start), point);
        float y1 = evaluate(multi.values().get(start + 1), point);
        float a = multi.derivatives()[start] * (x1 - x0) - (y1 - y0);
        float b = -multi.derivatives()[start + 1] * (x1 - x0) + (y1 - y0);
        return Mth.lerp(t, y0, y1) + t * (1.0F - t) * Mth.lerp(t, a, b);
    }

    record Constant<I extends BoundedFloatFunction<?>>(float value) implements CubicSpline<I> {
        public Interval range() {
            return Interval.ofExact(value);
        }

        public <O extends BoundedFloatFunction<?>> CubicSpline<O> mapCoordinates(Function<I, O> mapper) {
            return new Constant<>(value);
        }

        public void forEachCoordinate(Consumer<I> visitor) {
        }
    }

    record Multipoint<I extends BoundedFloatFunction<?>>(I coordinate, float[] locations, List<CubicSpline<I>> values,
        float[] derivatives) implements CubicSpline<I> {
        public Multipoint {
            if (locations.length == 0 || locations.length != values.size() || locations.length != derivatives.length) {
                throw new IllegalArgumentException("A spline needs matching locations, values and derivatives");
            }
        }

        int segment(float at) {
            int low = 0;
            int high = locations.length;
            while (low < high) {
                int mid = (low + high) >>> 1;
                if (at < locations[mid]) {
                    high = mid;
                } else {
                    low = mid + 1;
                }
            }
            return low - 1;
        }

        float extend(float at, float value, int index) {
            return derivatives[index] == 0.0F ? value : value + derivatives[index] * (at - locations[index]);
        }

        public Interval range() {
            Interval in = coordinate.range();
            if (in.isNaI()) {
                return in;
            }
            int last = locations.length - 1;
            float low = Float.POSITIVE_INFINITY;
            float high = Float.NEGATIVE_INFINITY;
            List<Interval> spans = new ArrayList<>(values.size());
            for (CubicSpline<I> value : values) {
                spans.add(value.range());
            }
            if (in.min() < locations[0]) {
                float a = extend(in.min(), spans.get(0).min(), 0);
                float b = extend(in.min(), spans.get(0).max(), 0);
                low = Math.min(low, Math.min(a, b));
                high = Math.max(high, Math.max(a, b));
            }
            if (in.max() > locations[last]) {
                float a = extend(in.max(), spans.get(last).min(), last);
                float b = extend(in.max(), spans.get(last).max(), last);
                low = Math.min(low, Math.min(a, b));
                high = Math.max(high, Math.max(a, b));
            }
            for (Interval span : spans) {
                low = Math.min(low, span.min());
                high = Math.max(high, span.max());
            }
            for (int i = 0; i < last; i++) {
                float d0 = derivatives[i];
                float d1 = derivatives[i + 1];
                if (d0 == 0.0F && d1 == 0.0F) {
                    continue;
                }
                float width = locations[i + 1] - locations[i];
                Interval s0 = spans.get(i);
                Interval s1 = spans.get(i + 1);
                float p0 = d0 * width;
                float p1 = d1 * width;
                float base = Math.min(s0.min(), s1.min());
                float top = Math.max(s0.max(), s1.max());
                float lowA = p0 - s1.max() + s0.min();
                float highA = p0 - s1.min() + s0.max();
                float lowB = -p1 + s1.min() - s0.max();
                float highB = -p1 + s1.max() - s0.min();
                low = Math.min(low, base + 0.25F * Math.min(lowA, lowB));
                high = Math.max(high, top + 0.25F * Math.max(highA, highB));
            }
            return Interval.of(low, high);
        }

        public <O extends BoundedFloatFunction<?>> CubicSpline<O> mapCoordinates(Function<I, O> mapper) {
            List<CubicSpline<O>> mapped = new ArrayList<>(values.size());
            for (CubicSpline<I> value : values) {
                mapped.add(value.mapCoordinates(mapper));
            }
            return new Multipoint<>(mapper.apply(coordinate), locations, mapped, derivatives);
        }

        public void forEachCoordinate(Consumer<I> visitor) {
            visitor.accept(coordinate);
            for (CubicSpline<I> value : values) {
                value.forEachCoordinate(visitor);
            }
        }
    }

    final class Builder<I extends BoundedFloatFunction<?>> {
        private final I coordinate;
        private final Float2FloatFunction valueTransformer;
        private final List<Float> locations = new ArrayList<>();
        private final List<CubicSpline<I>> values = new ArrayList<>();
        private final List<Float> derivatives = new ArrayList<>();

        private Builder(I coordinate, Float2FloatFunction valueTransformer) {
            this.coordinate = coordinate;
            this.valueTransformer = valueTransformer;
        }

        public Builder<I> addPoint(float location, float value) {
            return addPoint(location, new Constant<>(valueTransformer.get(value)), 0.0F);
        }

        public Builder<I> addPoint(float location, float value, float derivative) {
            return addPoint(location, new Constant<>(valueTransformer.get(value)), derivative);
        }

        public Builder<I> addPoint(float location, CubicSpline<I> value) {
            return addPoint(location, value, 0.0F);
        }

        public Builder<I> addPoint(float location, CubicSpline<I> value, float derivative) {
            if (!locations.isEmpty() && location <= locations.get(locations.size() - 1)) {
                throw new IllegalArgumentException("Spline points must be added in increasing order");
            }
            locations.add(location);
            values.add(value);
            derivatives.add(derivative);
            return this;
        }

        public CubicSpline<I> build() {
            if (locations.isEmpty()) {
                throw new IllegalStateException("A spline needs at least one point");
            }
            float[] at = new float[locations.size()];
            float[] slopes = new float[at.length];
            for (int i = 0; i < at.length; i++) {
                at[i] = locations.get(i);
                slopes[i] = derivatives.get(i);
            }
            return new Multipoint<>(coordinate, at, List.copyOf(values), slopes);
        }
    }
}
