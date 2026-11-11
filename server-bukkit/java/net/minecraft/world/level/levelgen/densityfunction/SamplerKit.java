// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.densityfunction;

/** Volume plumbing shared by the arithmetic samplers. */
public final class SamplerKit {
    private SamplerKit() {}

    public interface Op1 {
        float apply(float value);
    }

    public interface Op2 {
        float apply(float first, float second);
    }

    public static void map(SamplerContext context, DensitySampler input, DensityBuffer out, DensityVolume volume, Op1 op) {
        input.sampleVolume(context, out, volume);
        for (int i = 0; i < out.size(); i++) {
            out.set(i, op.apply(out.get(i)));
        }
    }

    public static void zip(SamplerContext context, DensitySampler first, DensitySampler second, DensityBuffer out,
        DensityVolume volume, Op2 op) {
        first.sampleVolume(context, out, volume);
        try (ScopedDensityBuffer other = context.acquireBuffer(volume)) {
            second.sampleVolume(context, other, volume);
            for (int i = 0; i < out.size(); i++) {
                out.set(i, op.apply(out.get(i), other.get(i)));
            }
        }
    }

    public static float[] floats(java.util.List<Float> values) {
        float[] out = new float[values.size()];
        for (int i = 0; i < out.length; i++) {
            out[i] = values.get(i);
        }
        return out;
    }

    public static String number(float value) {
        return Float.isFinite(value) ? Float.toString(value) : Float.isNaN(value) ? "0.0" : value > 0 ? "3.4028235E38" : "-3.4028235E38";
    }
}
