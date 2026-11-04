// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;

import java.util.Locale;
import java.util.UUID;
import java.util.function.IntPredicate;
import java.util.stream.IntStream;
import net.minecraft.core.Vec3i;
import net.minecraft.world.phys.AABB;
import net.minecraft.world.phys.Vec3;
import org.apache.commons.lang3.math.Fraction;
import org.apache.commons.lang3.math.NumberUtils;
import org.joml.Quaternionf;
import org.joml.Vector3f;
import org.joml.Vector3fc;

public class Mth {
    public static final float PI = (float) Math.PI;
    public static final float HALF_PI = PI / 2.0F;
    public static final float TWO_PI = PI * 2.0F;
    public static final float DEG_TO_RAD = PI / 180.0F;
    public static final float RAD_TO_DEG = 180.0F / PI;
    public static final float EPSILON = 1.0E-5F;
    public static final float SQRT_OF_TWO = (float) Math.sqrt(2.0);
    public static final Vector3fc Y_AXIS = new Vector3f(0.0F, 1.0F, 0.0F);
    public static final Vector3fc X_AXIS = new Vector3f(1.0F, 0.0F, 0.0F);
    public static final Vector3fc Z_AXIS = new Vector3f(0.0F, 0.0F, 1.0F);

    private static final int kSinSteps = 65536;
    private static final int kSinMask = kSinSteps - 1;
    private static final int kCosShift = kSinSteps / 4;
    private static final double kSinScale = 10430.378350470453;
    private static final float[] kSinTable = new float[kSinSteps];
    private static final int[] kLog2Debruijn = {0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8, 31, 27, 13,
        23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9};
    private static final double kFractionBias = Double.longBitsToDouble(4805340802404319232L);
    private static final double[] kAsinTable = new double[257];
    private static final double[] kCosOfAsinTable = new double[257];

    static {
        for (int i = 0; i < kSinSteps; i++) {
            kSinTable[i] = (float) Math.sin(i / kSinScale);
        }
        for (int i = 0; i <= 256; i++) {
            double asin = Math.asin(i / 256.0);
            kAsinTable[i] = asin;
            kCosOfAsinTable[i] = Math.cos(asin);
        }
    }

    protected Mth() {
    }

    public static float sin(double i) {
        return kSinTable[(int) ((long) (i * kSinScale) & kSinMask)];
    }

    public static float cos(double i) {
        return kSinTable[(int) ((long) (i * kSinScale + kCosShift) & kSinMask)];
    }

    public static float sqrt(float x) {
        return (float) Math.sqrt(x);
    }

    public static int floor(float v) {
        return (int) Math.floor(v);
    }

    public static int floor(double v) {
        return (int) Math.floor(v);
    }

    public static long lfloor(double v) {
        return (long) Math.floor(v);
    }

    public static float abs(float v) {
        return Math.abs(v);
    }

    public static int abs(int v) {
        return Math.abs(v);
    }

    public static int ceil(float v) {
        return (int) Math.ceil(v);
    }

    public static int ceil(double v) {
        return (int) Math.ceil(v);
    }

    public static long ceilLong(double v) {
        return (long) Math.ceil(v);
    }

    public static int clamp(int value, int min, int max) {
        return Math.min(Math.max(value, min), max);
    }

    public static long clamp(long value, long min, long max) {
        return Math.min(Math.max(value, min), max);
    }

    public static float clamp(float value, float min, float max) {
        return value < min ? min : Math.min(value, max);
    }

    public static double clamp(double value, double min, double max) {
        return value < min ? min : Math.min(value, max);
    }

    public static double clampedLerp(double factor, double min, double max) {
        return factor < 0.0 ? min : factor > 1.0 ? max : lerp(factor, min, max);
    }

    public static float clampedLerp(float factor, float min, float max) {
        return factor < 0.0F ? min : factor > 1.0F ? max : lerp(factor, min, max);
    }

    public static int absMax(int a, int b) {
        return Math.max(Math.abs(a), Math.abs(b));
    }

    public static float absMax(float a, float b) {
        return Math.max(Math.abs(a), Math.abs(b));
    }

    public static double absMax(double a, double b) {
        return Math.max(Math.abs(a), Math.abs(b));
    }

    public static int chessboardDistance(int x0, int z0, int x1, int z1) {
        return absMax(x1 - x0, z1 - z0);
    }

    public static int floorDiv(int a, int b) {
        return Math.floorDiv(a, b);
    }

    public static int nextInt(RandomSource random, int minInclusive, int maxInclusive) {
        return minInclusive >= maxInclusive ? minInclusive : random.nextInt(maxInclusive - minInclusive + 1) + minInclusive;
    }

    public static float nextFloat(RandomSource random, float min, float max) {
        return min >= max ? min : random.nextFloat() * (max - min) + min;
    }

    public static double nextDouble(RandomSource random, double min, double max) {
        return min >= max ? min : random.nextDouble() * (max - min) + min;
    }

    public static boolean equal(float a, float b) {
        return Math.abs(b - a) < EPSILON;
    }

    public static boolean equal(double a, double b) {
        return Math.abs(b - a) < (double) EPSILON;
    }

    public static int positiveModulo(int input, int mod) {
        return Math.floorMod(input, mod);
    }

    public static float positiveModulo(float input, float mod) {
        return (input % mod + mod) % mod;
    }

    public static double positiveModulo(double input, double mod) {
        return (input % mod + mod) % mod;
    }

    public static boolean isMultipleOf(int dividend, int divisor) {
        return dividend % divisor == 0;
    }

    public static byte packDegrees(float angle) {
        return (byte) floor(angle * 256.0F / 360.0F);
    }

    public static float unpackDegrees(byte rot) {
        return (float) (rot * 360) / 256.0F;
    }

    public static int wrapDegrees(int angle) {
        int wrapped = angle % 360;
        if (wrapped >= 180) {
            wrapped -= 360;
        }
        return wrapped < -180 ? wrapped + 360 : wrapped;
    }

    public static float wrapDegrees(long angle) {
        float wrapped = (float) (angle % 360L);
        if (wrapped >= 180.0F) {
            wrapped -= 360.0F;
        }
        return wrapped < -180.0F ? wrapped + 360.0F : wrapped;
    }

    public static float wrapDegrees(float angle) {
        float wrapped = angle % 360.0F;
        if (wrapped >= 180.0F) {
            wrapped -= 360.0F;
        }
        return wrapped < -180.0F ? wrapped + 360.0F : wrapped;
    }

    public static double wrapDegrees(double angle) {
        double wrapped = angle % 360.0;
        if (wrapped >= 180.0) {
            wrapped -= 360.0;
        }
        return wrapped < -180.0 ? wrapped + 360.0 : wrapped;
    }

    public static float wrapDegrees90(float angle) {
        float wrapped = angle % 90.0F;
        if (wrapped >= 45.0F) {
            wrapped -= 90.0F;
        }
        return wrapped < -45.0F ? wrapped + 90.0F : wrapped;
    }

    public static float degreesDifference(float fromAngle, float toAngle) {
        return wrapDegrees(toAngle - fromAngle);
    }

    public static float degreesDifferenceAbs(float angleA, float angleB) {
        return abs(degreesDifference(angleA, angleB));
    }

    public static float rotateIfNecessary(float baseAngle, float targetAngle, float maxAngleDiff) {
        return targetAngle - clamp(degreesDifference(baseAngle, targetAngle), -maxAngleDiff, maxAngleDiff);
    }

    public static float approach(float current, float target, float increment) {
        float step = abs(increment);
        return current < target ? clamp(current + step, current, target) : clamp(current - step, target, current);
    }

    public static float approachDegrees(float current, float target, float increment) {
        return approach(current, current + degreesDifference(current, target), increment);
    }

    public static int getInt(String input, int def) {
        return NumberUtils.toInt(input, def);
    }

    public static int smallestEncompassingPowerOfTwo(int input) {
        int bits = input - 1;
        bits |= bits >> 1;
        bits |= bits >> 2;
        bits |= bits >> 4;
        bits |= bits >> 8;
        bits |= bits >> 16;
        return bits + 1;
    }

    public static int smallestSquareSide(int itemCount) {
        if (itemCount < 0) {
            throw new IllegalArgumentException("itemCount must be greater than or equal to zero");
        }
        return ceil(Math.sqrt(itemCount));
    }

    public static boolean isPowerOfTwo(int input) {
        return input != 0 && (input & input - 1) == 0;
    }

    public static boolean isPowerOfTwo(long input) {
        return input != 0L && (input & input - 1L) == 0L;
    }

    public static int ceillog2(int input) {
        int power = isPowerOfTwo(input) ? input : smallestEncompassingPowerOfTwo(input);
        return kLog2Debruijn[(int) ((long) power * 125613361L >> 27) & 31];
    }

    public static int log2(int input) {
        return ceillog2(input) - (isPowerOfTwo(input) ? 0 : 1);
    }

    public static float frac(float num) {
        return num - floor(num);
    }

    public static double frac(double num) {
        return num - lfloor(num);
    }

    @Deprecated
    public static long getSeed(Vec3i vec) {
        return getSeed(vec.getX(), vec.getY(), vec.getZ());
    }

    @Deprecated
    public static long getSeed(int x, int y, int z) {
        long seed = (long) (x * 3129871) ^ (long) z * 116129781L ^ (long) y;
        seed = seed * seed * 42317861L + seed * 11L;
        return seed >> 16;
    }

    public static UUID createInsecureUUID(RandomSource random) {
        long most = random.nextLong() & ~0xF000L | 0x4000L;
        long least = random.nextLong() & 0x3FFFFFFFFFFFFFFFL | Long.MIN_VALUE;
        return new UUID(most, least);
    }

    public static double inverseLerp(double value, double min, double max) {
        return (value - min) / (max - min);
    }

    public static float inverseLerp(float value, float min, float max) {
        return (value - min) / (max - min);
    }

    public static boolean rayIntersectsAABB(Vec3 rayStart, Vec3 rayDir, AABB box) {
        double cx = (box.minX + box.maxX) * 0.5;
        double ex = (box.maxX - box.minX) * 0.5;
        double dx = rayStart.x - cx;
        if (Math.abs(dx) > ex && dx * rayDir.x >= 0.0) {
            return false;
        }
        double cy = (box.minY + box.maxY) * 0.5;
        double ey = (box.maxY - box.minY) * 0.5;
        double dy = rayStart.y - cy;
        if (Math.abs(dy) > ey && dy * rayDir.y >= 0.0) {
            return false;
        }
        double cz = (box.minZ + box.maxZ) * 0.5;
        double ez = (box.maxZ - box.minZ) * 0.5;
        double dz = rayStart.z - cz;
        if (Math.abs(dz) > ez && dz * rayDir.z >= 0.0) {
            return false;
        }
        double ax = Math.abs(rayDir.x);
        double ay = Math.abs(rayDir.y);
        double az = Math.abs(rayDir.z);
        double cross = rayDir.y * dz - rayDir.z * dy;
        if (Math.abs(cross) > ey * az + ez * ay) {
            return false;
        }
        cross = rayDir.z * dx - rayDir.x * dz;
        if (Math.abs(cross) > ex * az + ez * ax) {
            return false;
        }
        cross = rayDir.x * dy - rayDir.y * dx;
        return Math.abs(cross) < ex * ay + ey * ax;
    }

    public static double atan2(double y, double x) {
        double lengthSq = x * x + y * y;
        if (Double.isNaN(lengthSq)) {
            return Double.NaN;
        }
        boolean flipY = y < 0.0;
        double ay = flipY ? -y : y;
        boolean flipX = x < 0.0;
        double ax = flipX ? -x : x;
        boolean swap = ay > ax;
        if (swap) {
            double tmp = ax;
            ax = ay;
            ay = tmp;
        }
        double inv = fastInvSqrt(lengthSq);
        ax *= inv;
        ay *= inv;
        double biased = kFractionBias + ay;
        int index = (int) Double.doubleToRawLongBits(biased);
        double base = kAsinTable[index];
        double sinBase = biased - kFractionBias;
        double residual = ay * kCosOfAsinTable[index] - ax * sinBase;
        double correction = (6.0 + residual * residual) * residual * (1.0 / 6.0);
        double angle = base + correction;
        if (swap) {
            angle = Math.PI / 2.0 - angle;
        }
        if (flipX) {
            angle = Math.PI - angle;
        }
        return flipY ? -angle : angle;
    }

    public static float invSqrt(float x) {
        return org.joml.Math.invsqrt(x);
    }

    public static double invSqrt(double x) {
        return org.joml.Math.invsqrt(x);
    }

    @Deprecated
    public static double fastInvSqrt(double x) {
        long bits = 6910469410427058090L - (Double.doubleToRawLongBits(x) >> 1);
        double guess = Double.longBitsToDouble(bits);
        return guess * (1.5 - 0.5 * x * guess * guess);
    }

    public static float fastInvCubeRoot(float x) {
        float y = Float.intBitsToFloat(1419967116 - Float.floatToIntBits(x) / 3);
        y = 0.6666667F * y + 1.0F / (3.0F * y * y * x);
        return 0.6666667F * y + 1.0F / (3.0F * y * y * x);
    }

    public static int hsvToRgb(float hue, float saturation, float value) {
        return hsvToArgb(hue, saturation, value, 0);
    }

    public static int hsvToArgb(float hue, float saturation, float value, int alpha) {
        int sector = (int) (hue * 6.0F) % 6;
        float frac = hue * 6.0F - sector;
        float low = value * (1.0F - saturation);
        float down = value * (1.0F - frac * saturation);
        float up = value * (1.0F - (1.0F - frac) * saturation);
        float[] rgb = switch (sector) {
            case 0 -> new float[] {value, up, low};
            case 1 -> new float[] {down, value, low};
            case 2 -> new float[] {low, value, up};
            case 3 -> new float[] {low, down, value};
            case 4 -> new float[] {up, low, value};
            case 5 -> new float[] {value, low, down};
            default -> throw new RuntimeException("Something went wrong when converting from HSV to RGB. Input was "
                + hue + ", " + saturation + ", " + value);
        };
        int r = clamp((int) (rgb[0] * 255.0F), 0, 255);
        int g = clamp((int) (rgb[1] * 255.0F), 0, 255);
        int b = clamp((int) (rgb[2] * 255.0F), 0, 255);
        return alpha << 24 | r << 16 | g << 8 | b;
    }

    public static int murmurHash3Mixer(int hash) {
        hash ^= hash >>> 16;
        hash *= -2048144789;
        hash ^= hash >>> 13;
        hash *= -1028477387;
        return hash ^ hash >>> 16;
    }

    public static int binarySearch(int from, int to, IntPredicate condition) {
        int low = from;
        int remaining = to - from;
        while (remaining > 0) {
            int half = remaining / 2;
            int mid = low + half;
            if (condition.test(mid)) {
                remaining = half;
            } else {
                low = mid + 1;
                remaining -= half + 1;
            }
        }
        return low;
    }

    public static int lerpInt(float alpha1, int p0, int p1) {
        return p0 + floor(alpha1 * (p1 - p0));
    }

    public static int lerpDiscrete(float alpha1, int p0, int p1) {
        return p0 + floor(alpha1 * (p1 - p0 - 1)) + (alpha1 > 0.0F ? 1 : 0);
    }

    public static float lerp(float alpha1, float p0, float p1) {
        return p0 + alpha1 * (p1 - p0);
    }

    public static float lerp2(float alpha1, float alpha2, float x00, float x10, float x01, float x11) {
        return lerp(alpha2, lerp(alpha1, x00, x10), lerp(alpha1, x01, x11));
    }

    public static float lerp3(float alpha1, float alpha2, float alpha3, float x000, float x100, float x010, float x110,
        float x001, float x101, float x011, float x111) {
        return lerp(alpha3, lerp2(alpha1, alpha2, x000, x100, x010, x110), lerp2(alpha1, alpha2, x001, x101, x011, x111));
    }

    public static Vec3 lerp(double alpha, Vec3 p1, Vec3 p2) {
        return new Vec3(lerp(alpha, p1.x, p2.x), lerp(alpha, p1.y, p2.y), lerp(alpha, p1.z, p2.z));
    }

    public static double lerp(double alpha1, double p0, double p1) {
        return p0 + alpha1 * (p1 - p0);
    }

    public static double lerp2(double alpha1, double alpha2, double x00, double x10, double x01, double x11) {
        return lerp(alpha2, lerp(alpha1, x00, x10), lerp(alpha1, x01, x11));
    }

    public static double lerp3(double alpha1, double alpha2, double alpha3, double x000, double x100, double x010,
        double x110, double x001, double x101, double x011, double x111) {
        return lerp(alpha3, lerp2(alpha1, alpha2, x000, x100, x010, x110), lerp2(alpha1, alpha2, x001, x101, x011, x111));
    }

    public static float catmullrom(float alpha, float p0, float p1, float p2, float p3) {
        float a2 = alpha * alpha;
        return 0.5F * (2.0F * p1 + (p2 - p0) * alpha + (2.0F * p0 - 5.0F * p1 + 4.0F * p2 - p3) * a2
            + (3.0F * p1 - p0 - 3.0F * p2 + p3) * a2 * alpha);
    }

    public static float smoothstep(float x) {
        return x * x * x * (x * (x * 6.0F - 15.0F) + 10.0F);
    }

    public static float smoothstepDerivative(float x) {
        return 30.0F * x * x * (x - 1.0F) * (x - 1.0F);
    }

    public static int sign(double number) {
        return number == 0.0 ? 0 : number > 0.0 ? 1 : -1;
    }

    public static float rotLerp(float a, float from, float to) {
        return from + a * wrapDegrees(to - from);
    }

    public static double rotLerp(double a, double from, double to) {
        return from + a * wrapDegrees(to - from);
    }

    public static float rotLerpRad(float a, float from, float to) {
        float diff = to - from;
        while (diff < -PI) {
            diff += TWO_PI;
        }
        while (diff >= PI) {
            diff -= TWO_PI;
        }
        return from + a * diff;
    }

    public static float triangleWave(float index, float period) {
        return (Math.abs(index % period - period * 0.5F) - period * 0.25F) / (period * 0.25F);
    }

    public static float square(float x) {
        return x * x;
    }

    public static float cube(float x) {
        return x * x * x;
    }

    public static double square(double x) {
        return x * x;
    }

    public static int square(int x) {
        return x * x;
    }

    public static long square(long x) {
        return x * x;
    }

    public static double clampedMap(double value, double fromMin, double fromMax, double toMin, double toMax) {
        return clampedLerp(inverseLerp(value, fromMin, fromMax), toMin, toMax);
    }

    public static float clampedMap(float value, float fromMin, float fromMax, float toMin, float toMax) {
        return clampedLerp(inverseLerp(value, fromMin, fromMax), toMin, toMax);
    }

    public static double map(double value, double fromMin, double fromMax, double toMin, double toMax) {
        return lerp(inverseLerp(value, fromMin, fromMax), toMin, toMax);
    }

    public static float map(float value, float fromMin, float fromMax, float toMin, float toMax) {
        return lerp(inverseLerp(value, fromMin, fromMax), toMin, toMax);
    }

    public static double wobble(double coord) {
        double noise = RandomSource.createThreadLocalInstance(floor(coord * 3000.0)).nextDouble();
        return coord + (2.0 * noise - 1.0) * 1.0E-7 / 2.0;
    }

    public static int roundToward(int input, int multiple) {
        return positiveCeilDiv(input, multiple) * multiple;
    }

    public static long roundToward(long input, long multiple) {
        return positiveCeilDiv(input, multiple) * multiple;
    }

    public static int positiveCeilDiv(int input, int divisor) {
        return -Math.floorDiv(-input, divisor);
    }

    public static long positiveCeilDiv(long input, long divisor) {
        return -Math.floorDiv(-input, divisor);
    }

    public static int randomBetweenInclusive(RandomSource random, int min, int maxInclusive) {
        return random.nextInt(maxInclusive - min + 1) + min;
    }

    public static float randomBetween(RandomSource random, float min, float maxExclusive) {
        return random.nextFloat() * (maxExclusive - min) + min;
    }

    public static float normal(RandomSource random, float mean, float deviation) {
        return mean + (float) random.nextGaussian() * deviation;
    }

    public static double lengthSquared(double x, double y) {
        return x * x + y * y;
    }

    public static float lengthSquared(float x, float y) {
        return x * x + y * y;
    }

    public static double length(double x, double y) {
        return Math.sqrt(lengthSquared(x, y));
    }

    public static float length(float x, float y) {
        return (float) Math.sqrt(lengthSquared(x, y));
    }

    public static double lengthSquared(double x, double y, double z) {
        return x * x + y * y + z * z;
    }

    public static double length(double x, double y, double z) {
        return Math.sqrt(lengthSquared(x, y, z));
    }

    public static float length(float x, float y, float z) {
        return sqrt(lengthSquared(x, y, z));
    }

    public static float lengthSquared(float x, float y, float z) {
        return x * x + y * y + z * z;
    }

    public static int quantize(double value, int quantizeResolution) {
        return floor(value / quantizeResolution) * quantizeResolution;
    }

    public static IntStream outFromOrigin(int origin, int lowerBound, int upperBound) {
        return outFromOrigin(origin, lowerBound, upperBound, 1);
    }

    public static IntStream outFromOrigin(int origin, int lowerBound, int upperBound, int stepSize) {
        if (lowerBound > upperBound) {
            throw new IllegalArgumentException(String.format(Locale.ROOT, "upperBound %d expected to be > lowerBound %d",
                upperBound, lowerBound));
        }
        if (stepSize < 1) {
            throw new IllegalArgumentException(String.format(Locale.ROOT, "step size expected to be >= 1, was %d", stepSize));
        }
        int start = clamp(origin, lowerBound, upperBound);
        return IntStream.iterate(start, cursor -> {
            int distance = Math.abs(start - cursor);
            return start - distance >= lowerBound || start + distance <= upperBound;
        }, cursor -> {
            boolean wasBelow = cursor <= start;
            int distance = Math.abs(start - cursor);
            boolean canGoUp = start + distance + stepSize <= upperBound;
            if (!wasBelow || !canGoUp) {
                int down = start - distance - (wasBelow ? stepSize : 0);
                if (down >= lowerBound) {
                    return down;
                }
            }
            return start + distance + stepSize;
        });
    }

    public static Quaternionf rotationAroundAxis(Vector3fc axis, Quaternionf rotation, Quaternionf result) {
        float projected = axis.dot(rotation.x, rotation.y, rotation.z);
        return result.set(axis.x() * projected, axis.y() * projected, axis.z() * projected, rotation.w).normalize();
    }

    public static int mulAndTruncate(Fraction fraction, int factor) {
        return fraction.getNumerator() * factor / fraction.getDenominator();
    }
}
