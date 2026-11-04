// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.valueproviders;

import com.mojang.serialization.MapCodec;

public interface FloatProvider extends SampledFloat {
    float min();

    float max();

    MapCodec<? extends FloatProvider> codec();
}
