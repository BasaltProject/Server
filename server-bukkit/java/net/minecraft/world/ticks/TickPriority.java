// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.ticks;

import com.mojang.serialization.Codec;

public enum TickPriority {
    EXTREMELY_HIGH(-3), VERY_HIGH(-2), HIGH(-1), NORMAL(0), LOW(1), VERY_LOW(2), EXTREMELY_LOW(3);

    public static final Codec<TickPriority> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.ticks.TickPriority.class, "CODEC");

    private final int value;

    TickPriority(int value) {
        this.value = value;
    }

    public static TickPriority byValue(int value) {
        for (TickPriority priority : values()) {
            if (priority.value == value) {
                return priority;
            }
        }
        return value < EXTREMELY_HIGH.value ? EXTREMELY_HIGH : EXTREMELY_LOW;
    }

    public int getValue() {
        return value;
    }
}
