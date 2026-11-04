// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.valueproviders;

import net.minecraft.util.RandomSource;

public interface SampledFloat {
    float sample(RandomSource random);
}
