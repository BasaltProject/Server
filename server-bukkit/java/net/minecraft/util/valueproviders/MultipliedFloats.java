// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.valueproviders;

import net.minecraft.util.RandomSource;

public class MultipliedFloats implements SampledFloat {
    private final SampledFloat[] values;

    public MultipliedFloats(SampledFloat... values) {
        this.values = values;
    }

    public float sample(RandomSource random) {
        float result = 1.0F;
        for (SampledFloat value : values) {
            result *= value.sample(random);
        }
        return result;
    }
}
