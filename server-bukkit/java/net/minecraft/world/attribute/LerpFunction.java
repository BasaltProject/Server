// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.attribute;

import java.util.ArrayList;
import java.util.List;
import org.joml.Vector3f;
import org.joml.Vector3fc;
import org.joml.Vector4f;
import org.joml.Vector4fc;

public interface LerpFunction<T> {
    LerpFunction<?> CONSTANT = (alpha, from, to) -> from;

    static LerpFunction<Float> ofFloat() {
        return (alpha, from, to) -> from + (to - from) * alpha;
    }

    static LerpFunction<Integer> ofInteger() {
        return (alpha, from, to) -> Math.round(from + (to - from) * alpha);
    }

    static LerpFunction<Float> ofDegrees(float maxDelta) {
        return (alpha, from, to) -> {
            float delta = ((to - from) % 360.0F + 540.0F) % 360.0F - 180.0F;
            return from + Math.max(-maxDelta, Math.min(maxDelta, delta)) * alpha;
        };
    }

    @SuppressWarnings("unchecked")
    static <T> LerpFunction<T> ofConstant() {
        return (LerpFunction<T>) CONSTANT;
    }

    static <T> LerpFunction<T> ofStep(float threshold) {
        return (alpha, from, to) -> alpha >= threshold ? to : from;
    }

    static LerpFunction<Integer> ofColor() {
        return (alpha, from, to) -> {
            int out = 0;
            for (int shift = 0; shift < 32; shift += 8) {
                int a = (from >>> shift) & 0xFF;
                int b = (to >>> shift) & 0xFF;
                out |= (Math.round(a + (b - a) * alpha) & 0xFF) << shift;
            }
            return out;
        };
    }

    static LerpFunction<Vector3fc> ofColorVec3() {
        return (alpha, from, to) -> from.lerp(to, alpha, new Vector3f());
    }

    static LerpFunction<Vector4fc> ofColorVec4() {
        return (alpha, from, to) -> from.lerp(to, alpha, new Vector4f());
    }

    static <T> LerpFunction<List<T>> ofListCrossFade(AlphaScaler<T> scaler) {
        return (alpha, from, to) -> {
            List<T> mixed = new ArrayList<>(from.size() + to.size());
            for (T item : from) {
                mixed.add(scaler.apply(item, 1.0F - alpha));
            }
            for (T item : to) {
                mixed.add(scaler.apply(item, alpha));
            }
            return mixed;
        };
    }

    T apply(float alpha, T from, T to);

    interface AlphaScaler<T> {
        T apply(T item, float alpha);
    }
}
