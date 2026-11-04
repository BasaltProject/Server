// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;
public class Ease {
    private static final float kBack1 = 1.70158F;
    private static final float kBack2 = 2.5949094F;
    private static final float kBack3 = 2.70158F;
    private static final float kElastic1 = (float) (2.0 * Math.PI / 3.0);
    private static final float kElastic2 = (float) (2.0 * Math.PI / 4.5);
    public static float inBack(float x) {
        return Mth.square(x) * (kBack3 * x - kBack1);
    }
    public static float inBounce(float x) {
        return 1.0F - outBounce(1.0F - x);
    }
    public static float inCubic(float x) {
        return Mth.cube(x);
    }
    public static float inElastic(float x) {
        if (x == 0.0F || x == 1.0F) {
            return x;
        }
        return (float) (-Math.pow(2.0, 10.0 * x - 10.0) * Math.sin((x * 10.0 - 10.75) * kElastic1));
    }
    public static float inExpo(float x) {
        return x == 0.0F ? 0.0F : (float) Math.pow(2.0, 10.0 * x - 10.0);
    }
    public static float inQuart(float x) {
        return Mth.square(Mth.square(x));
    }
    public static float inQuint(float x) {
        return Mth.square(Mth.square(x)) * x;
    }
    public static float inSine(float x) {
        return 1.0F - Mth.cos((double) (x * ((float) Math.PI / 2F)));
    }
    public static float inOutBounce(float x) {
        return x < 0.5F ? (1.0F - outBounce(1.0F - 2.0F * x)) / 2.0F : (1.0F + outBounce(2.0F * x - 1.0F)) / 2.0F;
    }
    public static float inOutCirc(float x) {
        return x < 0.5F ? (float) ((1.0 - Math.sqrt(1.0 - Math.pow(2.0 * x, 2.0))) / 2.0)
            : (float) ((Math.sqrt(1.0 - Math.pow(-2.0 * x + 2.0, 2.0)) + 1.0) / 2.0);
    }
    public static float inOutCubic(float x) {
        return x < 0.5F ? 4.0F * Mth.cube(x) : (float) (1.0 - Math.pow(-2.0 * x + 2.0, 3.0) / 2.0);
    }
    public static float inOutQuad(float x) {
        return x < 0.5F ? 2.0F * Mth.square(x) : (float) (1.0 - Math.pow(-2.0 * x + 2.0, 2.0) / 2.0);
    }
    public static float inOutQuart(float x) {
        return x < 0.5F ? 8.0F * Mth.square(Mth.square(x)) : (float) (1.0 - Math.pow(-2.0 * x + 2.0, 4.0) / 2.0);
    }
    public static float inOutQuint(float x) {
        return x < 0.5F ? 16.0F * x * x * x * x * x : (float) (1.0 - Math.pow(-2.0 * x + 2.0, 5.0) / 2.0);
    }
    public static float outBounce(float x) {
        final float n1 = 7.5625F;
        if (x < 1.0F / 2.75F) {
            return n1 * Mth.square(x);
        }
        if (x < 2.0F / 2.75F) {
            return n1 * Mth.square(x - 1.5F / 2.75F) + 0.75F;
        }
        if (x < 2.5F / 2.75F) {
            return n1 * Mth.square(x - 2.25F / 2.75F) + 0.9375F;
        }
        return n1 * Mth.square(x - 2.625F / 2.75F) + 0.984375F;
    }
    public static float outElastic(float x) {
        if (x == 0.0F || x == 1.0F) {
            return x;
        }
        return (float) (Math.pow(2.0, -10.0 * x) * Math.sin((x * 10.0 - 0.75) * kElastic1) + 1.0);
    }
    public static float outExpo(float x) {
        return x == 1.0F ? 1.0F : 1.0F - (float) Math.pow(2.0, -10.0 * x);
    }
    public static float outQuad(float x) {
        return 1.0F - Mth.square(1.0F - x);
    }
    public static float outQuint(float x) {
        return 1.0F - (float) Math.pow(1.0 - x, 5.0);
    }
    public static float outSine(float x) {
        return Mth.sin((double) (x * ((float) Math.PI / 2F)));
    }
    public static float inOutSine(float x) {
        return -(Mth.cos((double) ((float) Math.PI * x)) - 1.0F) / 2.0F;
    }
    public static float outBack(float x) {
        return 1.0F + kBack3 * Mth.cube(x - 1.0F) + kBack1 * Mth.square(x - 1.0F);
    }
    public static float outQuart(float x) {
        return 1.0F - Mth.square(Mth.square(1.0F - x));
    }
    public static float outCubic(float x) {
        return 1.0F - Mth.cube(1.0F - x);
    }
    public static float inOutExpo(float x) {
        if (x < 0.5F) {
            return x == 0.0F ? 0.0F : (float) (Math.pow(2.0, 20.0 * x - 10.0) / 2.0);
        }
        return x == 1.0F ? 1.0F : (float) ((2.0 - Math.pow(2.0, -20.0 * x + 10.0)) / 2.0);
    }
    public static float inQuad(float x) {
        return x * x;
    }
    public static float outCirc(float x) {
        return (float) Math.sqrt(1.0F - Mth.square(x - 1.0F));
    }
    public static float inOutElastic(float x) {
        if (x == 0.0F || x == 1.0F) {
            return x;
        }
        double sin = Math.sin((20.0 * x - 11.125) * kElastic2);
        return x < 0.5F ? (float) (-(Math.pow(2.0, 20.0 * x - 10.0) * sin) / 2.0) : (float) (Math.pow(2.0, -20.0 * x + 10.0) * sin / 2.0 + 1.0);
    }
    public static float inCirc(float x) {
        return (float) -Math.sqrt(1.0F - x * x) + 1.0F;
    }
    public static float inOutBack(float x) {
        if (x < 0.5F) {
            return 4.0F * x * x * (7.189819F * x - kBack2) / 2.0F;
        }
        float t = 2.0F * x - 2.0F;
        return (t * t * (3.5949094F * t + kBack2) + 2.0F) / 2.0F;
    }
}
