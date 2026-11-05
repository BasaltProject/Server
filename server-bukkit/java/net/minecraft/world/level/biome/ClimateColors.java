// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.biome;

import net.minecraft.util.Mth;

/** Tint for biomes that name no color; a three-corner blend that approximates the climate colormaps. */
final class ClimateColors {
    static final int kDryFoliage = 0x9E814D;
    private static final int[] kGrass = {0x80B497, 0xBFB755, 0x59AE30};
    private static final int[] kFoliage = {0x619961, 0xAEA42A, 0x30BB0B};

    private ClimateColors() {}

    static int grass(float temperature, float downfall) {
        return blend(kGrass, temperature, downfall);
    }

    static int foliage(float temperature, float downfall) {
        return blend(kFoliage, temperature, downfall);
    }

    private static int blend(int[] corners, float temperature, float downfall) {
        float warm = Mth.clamp(temperature, 0.0F, 1.0F);
        float wet = Mth.clamp(downfall, 0.0F, 1.0F) * warm;
        float[] weight = {1.0F - warm, warm - wet, wet};
        int out = 0;
        for (int shift = 16; shift >= 0; shift -= 8) {
            float channel = 0.0F;
            for (int i = 0; i < 3; i++) {
                channel += weight[i] * ((corners[i] >> shift) & 0xFF);
            }
            out |= Math.round(channel) << shift;
        }
        return 0xFF000000 | out;
    }
}
