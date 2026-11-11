// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;

import com.mojang.datafixers.util.Either;
import com.mojang.serialization.Codec;
import com.mojang.serialization.DataResult;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.List;

public interface EasingType {
    ExtraCodecs.LateBoundIdMapper<String, EasingType> SIMPLE_REGISTRY = new ExtraCodecs.LateBoundIdMapper<>();
    Codec<EasingType> CODEC = Codec.either(SIMPLE_REGISTRY.codec(Codec.STRING), CubicBezier.CODEC).xmap(Either::unwrap,
        easing -> easing instanceof CubicBezier bezier ? Either.right(bezier) : Either.left(easing));
    EasingType CONSTANT = simple("constant", x -> 0.0F);
    EasingType LINEAR = simple("linear", x -> x);
    EasingType IN_BACK = simple("in_back", Ease::inBack);
    EasingType IN_BOUNCE = simple("in_bounce", Ease::inBounce);
    EasingType IN_CIRC = simple("in_circ", Ease::inCirc);
    EasingType IN_CUBIC = simple("in_cubic", Ease::inCubic);
    EasingType IN_ELASTIC = simple("in_elastic", Ease::inElastic);
    EasingType IN_EXPO = simple("in_expo", Ease::inExpo);
    EasingType IN_QUAD = simple("in_quad", Ease::inQuad);
    EasingType IN_QUART = simple("in_quart", Ease::inQuart);
    EasingType IN_QUINT = simple("in_quint", Ease::inQuint);
    EasingType IN_SINE = simple("in_sine", Ease::inSine);
    EasingType IN_OUT_BACK = simple("in_out_back", Ease::inOutBack);
    EasingType IN_OUT_BOUNCE = simple("in_out_bounce", Ease::inOutBounce);
    EasingType IN_OUT_CIRC = simple("in_out_circ", Ease::inOutCirc);
    EasingType IN_OUT_CUBIC = simple("in_out_cubic", Ease::inOutCubic);
    EasingType IN_OUT_ELASTIC = simple("in_out_elastic", Ease::inOutElastic);
    EasingType IN_OUT_EXPO = simple("in_out_expo", Ease::inOutExpo);
    EasingType IN_OUT_QUAD = simple("in_out_quad", Ease::inOutQuad);
    EasingType IN_OUT_QUART = simple("in_out_quart", Ease::inOutQuart);
    EasingType IN_OUT_QUINT = simple("in_out_quint", Ease::inOutQuint);
    EasingType IN_OUT_SINE = simple("in_out_sine", Ease::inOutSine);
    EasingType OUT_BACK = simple("out_back", Ease::outBack);
    EasingType OUT_BOUNCE = simple("out_bounce", Ease::outBounce);
    EasingType OUT_CIRC = simple("out_circ", Ease::outCirc);
    EasingType OUT_CUBIC = simple("out_cubic", Ease::outCubic);
    EasingType OUT_ELASTIC = simple("out_elastic", Ease::outElastic);
    EasingType OUT_EXPO = simple("out_expo", Ease::outExpo);
    EasingType OUT_QUAD = simple("out_quad", Ease::outQuad);
    EasingType OUT_QUART = simple("out_quart", Ease::outQuart);
    EasingType OUT_QUINT = simple("out_quint", Ease::outQuint);
    EasingType OUT_SINE = simple("out_sine", Ease::outSine);

    static EasingType simple(String id, EasingType easing) {
        SIMPLE_REGISTRY.put(id, easing);
        return easing;
    }

    static EasingType cubicBezier(float x1, float y1, float x2, float y2) {
        return new CubicBezier(new CubicBezierControls(x1, y1, x2, y2));
    }

    static EasingType symmetricCubicBezier(float x1, float y1) {
        return cubicBezier(x1, y1, 1.0F - x1, 1.0F - y1);
    }

    float apply(float x);

    public static final class CubicBezier implements EasingType {
        public static final Codec<CubicBezier> CODEC = RecordCodecBuilder.create(instance -> instance.group(
            CubicBezierControls.CODEC.fieldOf("cubic_bezier").forGetter(bezier -> bezier.controls)).apply(instance, CubicBezier::new));
        private static final float kTolerance = 1.0E-5F;
        private final CubicBezierControls controls;
        private final CubicCurve xCurve;
        private final CubicCurve yCurve;

        public CubicBezier(CubicBezierControls controls) {
            this.controls = controls;
            this.xCurve = curve(controls.x1, controls.x2);
            this.yCurve = curve(controls.y1, controls.y2);
        }

        private static CubicCurve curve(float p1, float p2) {
            return new CubicCurve(3.0F * p1 - 3.0F * p2 + 1.0F, -6.0F * p1 + 3.0F * p2, 3.0F * p1);
        }

        @Override
        public float apply(float x) {
            return yCurve.sample(solve(x));
        }

        private float solve(float x) {
            float t = x;
            for (int i = 0; i < 4; i++) {
                float error = xCurve.sample(t) - x;
                if (Math.abs(error) < kTolerance) {
                    return t;
                }
                float slope = xCurve.sampleGradient(t);
                if (slope < kTolerance) {
                    break;
                }
                t -= Mth.clamp(error / slope, -0.25F, 0.25F);
            }
            return bisect(x, t);
        }

        private float bisect(float x, float guess) {
            float low = 0.0F;
            float high = 1.0F;
            float t = guess;
            while (low < high) {
                float error = xCurve.sample(t) - x;
                if (Math.abs(error) < kTolerance) {
                    return t;
                }
                if (error < 0.0F) {
                    low = t;
                } else {
                    high = t;
                }
                t = (high + low) / 2.0F;
            }
            return t;
        }

        @Override
        public boolean equals(Object obj) {
            return obj instanceof CubicBezier other && controls.equals(other.controls);
        }

        @Override
        public int hashCode() {
            return controls.hashCode();
        }

        @Override
        public String toString() {
            return "CubicBezier(" + controls.x1 + ", " + controls.y1 + ", " + controls.x2 + ", " + controls.y2 + ")";
        }

        static record CubicCurve(float a, float b, float c) {
            public float sample(float t) {
                return ((a * t + b) * t + c) * t;
            }

            public float sampleGradient(float t) {
                return (3.0F * a * t + 2.0F * b) * t + c;
            }
        }
    }

    public static record CubicBezierControls(float x1, float y1, float x2, float y2) {
        public static final Codec<CubicBezierControls> CODEC = Codec.FLOAT.listOf(4, 4)
            .xmap(floats -> new CubicBezierControls(floats.get(0), floats.get(1), floats.get(2), floats.get(3)),
                controls -> List.of(controls.x1, controls.y1, controls.x2, controls.y2))
            .validate(CubicBezierControls::validate);

        private DataResult<CubicBezierControls> validate() {
            if (x1 < 0.0F || x1 > 1.0F) {
                return DataResult.error(() -> "x1 must be in range [0; 1]");
            }
            return x2 < 0.0F || x2 > 1.0F ? DataResult.error(() -> "x2 must be in range [0; 1]") : DataResult.success(this);
        }
    }
}
