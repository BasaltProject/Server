// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.attribute.modifier;

import com.mojang.serialization.Codec;

public record FloatWithAlpha(float value, float alpha) {
    public static final Codec<FloatWithAlpha> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(FloatWithAlpha.class, "CODEC");

    public FloatWithAlpha(float value) {
        this(value, 1.0F);
    }
}
