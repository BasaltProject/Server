// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;

import it.unimi.dsi.fastutil.floats.FloatUnaryOperator;
import java.util.List;

public final class Interval {
    public static final Interval NaI = new Interval(Float.NaN, Float.NaN);
    public static final Interval INFINITE = new Interval(Float.NEGATIVE_INFINITY, Float.POSITIVE_INFINITY);
    private static final Interval kSigned = new Interval(-1.0F, 1.0F);
    private static final Interval kUnit = new Interval(0.0F, 1.0F);
    private final float min;
    private final float max;

    private Interval(float min, float max) {
        this.min = min;
        this.max = max;
    }

    public static Interval of(float min, float max) {
        if (max < min) {
            throw new IllegalArgumentException("interval maximum " + max + " is below its minimum " + min);
        }
        if (Float.isNaN(min) || Float.isNaN(max)) {
            throw new IllegalArgumentException("NaN bound in [" + min + "; " + max + "], use the NaI constant");
        }
        if (min == Float.NEGATIVE_INFINITY && max == Float.POSITIVE_INFINITY) {
            return INFINITE;
        }
        if (max == 1.0F && min == 0.0F) {
            return kUnit;
        }
        if (max == 1.0F && min == -1.0F) {
            return kSigned;
        }
        return new Interval(min, max);
    }

    public static Interval ofSymmetric(float range) {
        return of(-range, range);
    }

    public static Interval ofExact(float value) {
        return of(value, value);
    }

    public static Interval encapsulating(List<Interval> intervals) {
        if (intervals.isEmpty()) {
            throw new IllegalArgumentException("encapsulating needs at least one interval");
        }
        float low = Float.POSITIVE_INFINITY;
        float high = Float.NEGATIVE_INFINITY;
        for (Interval interval : intervals) {
            if (!interval.isNaI()) {
                low = Math.min(low, interval.min);
                high = Math.max(high, interval.max);
            }
        }
        return high < low ? NaI : of(low, high);
    }

    public static Interval encapsulating(Interval... intervals) {
        return encapsulating(List.of(intervals));
    }

    public static Interval encapsulating(float first, float second) {
        if (Float.isNaN(first)) {
            return Float.isNaN(second) ? NaI : ofExact(second);
        }
        if (Float.isNaN(second)) {
            return ofExact(first);
        }
        return of(Math.min(first, second), Math.max(first, second));
    }

    private static Interval encapsulating(Interval base, float extra) {
        if (Float.isNaN(extra)) {
            return base;
        }
        return base.isNaI() ? ofExact(extra) : of(Math.min(base.min, extra), Math.max(base.max, extra));
    }

    private static Interval orNaI(float low, float high) {
        return Float.isNaN(low) || Float.isNaN(high) ? NaI : of(low, high);
    }

    public static Interval add(Interval left, Interval right) {
        return orNaI(left.min + right.min, left.max + right.max);
    }

    public static Interval sub(Interval left, Interval right) {
        return orNaI(left.min - right.max, left.max - right.min);
    }

    private static float product(float left, float right) {
        return left == 0.0F || right == 0.0F ? 0.0F : left * right;
    }

    public static Interval mul(Interval left, Interval right) {
        if (left.isNaI() || right.isNaI()) {
            return NaI;
        }
        float a = product(left.min, right.min);
        float b = product(left.min, right.max);
        float c = product(left.max, right.min);
        float d = product(left.max, right.max);
        return of(Math.min(Math.min(a, b), Math.min(c, d)), Math.max(Math.max(a, b), Math.max(c, d)));
    }

    public static Interval reciprocal(Interval input) {
        if (input.isNaI() || input.min == 0.0F && input.max == 0.0F) {
            return NaI;
        }
        if (!input.contains(0.0F)) {
            return of(1.0F / input.max, 1.0F / input.min);
        }
        if (input.max == 0.0F) {
            return of(Float.NEGATIVE_INFINITY, 1.0F / input.min);
        }
        return input.min == 0.0F ? of(1.0F / input.max, Float.POSITIVE_INFINITY) : INFINITE;
    }

    public static Interval div(Interval left, Interval right) {
        Interval inverse = reciprocal(right);
        return mul(left, inverse);
    }

    public static Interval min(Interval left, Interval right) {
        return left.isNaI() || right.isNaI() ? NaI : of(Math.min(left.min, right.min), Math.min(left.max, right.max));
    }

    public static Interval max(Interval left, Interval right) {
        return left.isNaI() || right.isNaI() ? NaI : of(Math.max(left.min, right.min), Math.max(left.max, right.max));
    }

    public static Interval clamp(Interval input, float min, float max) {
        if (min > max) {
            throw new IllegalArgumentException("interval minimum " + min + " is above its maximum " + max);
        }
        if (input.isNaI()) {
            return NaI;
        }
        if (input.min >= max) {
            return ofExact(max);
        }
        if (input.max <= min) {
            return ofExact(min);
        }
        return of(Math.max(input.min, min), Math.min(input.max, max));
    }

    public static Interval abs(Interval input) {
        if (input.isNaI()) {
            return NaI;
        }
        float far = Math.max(Math.abs(input.min), Math.abs(input.max));
        return of(input.contains(0.0F) ? 0.0F : Math.min(Math.abs(input.min), Math.abs(input.max)), far);
    }

    public static Interval square(Interval input) {
        if (input.isNaI()) {
            return NaI;
        }
        float far = Math.max(Mth.square(input.min), Mth.square(input.max));
        return of(input.contains(0.0F) ? 0.0F : Math.min(Mth.square(input.min), Mth.square(input.max)), far);
    }

    public static Interval pow(Interval base, Interval exponent) {
        if (base.isNaI() || exponent.isNaI()) {
            return NaI;
        }
        if (base.min == base.max) {
            return powOf(base.min, exponent);
        }
        Interval result = encapsulating(powOf(base.min, exponent), powOf(base.max, exponent));
        if (base.contains(0.0F)) {
            if (base.max > 0.0F) {
                result = encapsulating(result, powOf(0.0F, exponent));
            }
            if (base.min < 0.0F) {
                result = encapsulating(result, powOf(-0.0F, exponent));
            }
        }
        return result;
    }

    private static Interval encapsulating(Interval left, Interval right) {
        return encapsulating(List.of(left, right));
    }

    private static Interval powOf(float base, Interval exponent) {
        if (Float.isNaN(base)) {
            return NaI;
        }
        if (exponent.min == exponent.max) {
            float value = (float) Math.pow(base, exponent.min);
            if (Float.isNaN(value)) {
                return NaI;
            }
            return ofExact(value);
        }
        if (base == 0.0F) {
            return mul(powOfZero(exponent), ofExact(Math.copySign(1.0F, base)));
        }
        if (base == 1.0F) {
            return ofExact(1.0F);
        }
        return base > 0.0F ? powOfPositive(base, exponent) : powOfNegative(base, exponent);
    }

    private static Interval powOfZero(Interval exponent) {
        if (!exponent.contains(0.0F)) {
            return exponent.max < 0.0F ? ofExact(Float.POSITIVE_INFINITY) : ofExact(0.0F);
        }
        if (exponent.max == 0.0F) {
            float one = 1.0F;
            return of(one, Float.POSITIVE_INFINITY);
        }
        return exponent.min == 0.0F ? kUnit : of(0.0F, Float.POSITIVE_INFINITY);
    }

    private static Interval powOfPositive(float base, Interval exponent) {
        if (Float.isFinite(exponent.min) && Float.isFinite(exponent.max)) {
            return encapsulating((float) Math.pow(base, exponent.min), (float) Math.pow(base, exponent.max));
        }
        if (Float.isInfinite(exponent.min) && Float.isInfinite(exponent.max)) {
            return of(0.0F, Float.POSITIVE_INFINITY);
        }
        boolean shrinking = base < 1.0F;
        if (Float.isInfinite(exponent.min)) {
            float far = (float) Math.pow(base, exponent.max);
            return shrinking ? of(far, Float.POSITIVE_INFINITY) : of(0.0F, far);
        }
        float near = (float) Math.pow(base, exponent.min);
        return shrinking ? of(0.0F, near) : of(near, Float.POSITIVE_INFINITY);
    }

    private static Interval powOfNegative(float base, Interval exponent) {
        float lowest = (float) Math.ceil(exponent.min);
        float highest = (float) Math.floor(exponent.max);
        if (highest < lowest) {
            return NaI;
        }
        float atLowest = (float) Math.pow(base, lowest);
        float atHighest = (float) Math.pow(base, highest);
        Interval result = encapsulating(atLowest, atHighest);
        if (Float.isInfinite(lowest)) {
            result = encapsulating(result, -atLowest);
        } else if (lowest + 1.0F < highest) {
            result = encapsulating(result, (float) Math.pow(base, lowest + 1.0F));
        }
        if (Float.isInfinite(highest)) {
            result = encapsulating(result, -atHighest);
        } else if (highest - 1.0F > lowest) {
            result = encapsulating(result, (float) Math.pow(base, highest - 1.0F));
        }
        return result;
    }

    public static Interval log(Interval input) {
        if (input.max < 0.0F) {
            return NaI;
        }
        return mapMonotonic(max(input, ofExact(0.0F)), x -> (float) Math.log(x));
    }

    public static Interval mapMonotonic(Interval input, FloatUnaryOperator monotonicOp) {
        if (input.isNaI()) {
            return NaI;
        }
        float low = monotonicOp.apply(input.min);
        float high = monotonicOp.apply(input.max);
        if (Float.isNaN(low) || Float.isNaN(high)) {
            throw new IllegalStateException("A monotonic operator must not produce NaN");
        }
        return of(Math.min(low, high), Math.max(low, high));
    }

    public static Interval lerp(Interval alpha, Interval first, Interval second) {
        if (alpha.isNaI() || first.isNaI() || second.isNaI()) {
            return NaI;
        }
        return encapsulating(lerp(alpha, first.min, second.min), lerp(alpha, first.max, second.min),
            lerp(alpha, first.min, second.max), lerp(alpha, first.max, second.max));
    }

    public static Interval lerp(Interval alpha, float first, float second) {
        if (alpha.isNaI() || Float.isNaN(first) || Float.isNaN(second)) {
            return NaI;
        }
        if (Float.isFinite(first) && Float.isFinite(second)) {
            return encapsulating(first + product(alpha.min, second - first), first + product(alpha.max, second - first));
        }
        if (first == second) {
            return ofExact(first);
        }
        float low = lerpToInfinity(alpha.min, first, second);
        float high = lerpToInfinity(alpha.max, first, second);
        return Float.isNaN(low) || Float.isNaN(high) ? NaI : encapsulating(low, high);
    }

    private static float lerpToInfinity(float alpha, float first, float second) {
        float head = product(1.0F - alpha, first);
        float tail = product(alpha, second);
        if (!Float.isInfinite(head) || !Float.isInfinite(tail)) {
            return head + tail;
        }
        if (alpha <= 0.0F) {
            return first < second ? Float.NEGATIVE_INFINITY : Float.POSITIVE_INFINITY;
        }
        if (alpha >= 1.0F) {
            return second > first ? Float.POSITIVE_INFINITY : Float.NEGATIVE_INFINITY;
        }
        return Float.NaN;
    }

    public static Interval sign(Interval input) {
        if (input.isNaI()) {
            return NaI;
        }
        if (input.min == input.max) {
            return ofExact(Math.signum(input.min));
        }
        if (!input.contains(0.0F)) {
            return ofExact(input.min > 0.0F ? 1.0F : -1.0F);
        }
        if (input.min == 0.0F) {
            return kUnit;
        }
        return input.max == 0.0F ? of(-1.0F, 0.0F) : kSigned;
    }

    public boolean contains(float value) {
        return value >= min && value <= max;
    }

    public boolean intersects(Interval other) {
        return min <= other.max && max >= other.min;
    }

    public boolean isNaI() {
        return this == NaI;
    }

    public float min() {
        return min;
    }

    public float max() {
        return max;
    }

    public boolean equals(Object obj) {
        return obj == this || obj instanceof Interval other && min == other.min && max == other.max;
    }

    public int hashCode() {
        return Float.hashCode(min) * 31 + Float.hashCode(max);
    }

    public String toString() {
        return isNaI() ? "[NaN]" : "[" + min + "; " + max + "]";
    }
}
