// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.pools;
import com.mojang.serialization.Codec;
public record DimensionPadding(int bottom, int top) {
    public static final Codec<DimensionPadding> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.structure.pools.DimensionPadding.class, "CODEC");
    public static final DimensionPadding ZERO = new DimensionPadding(0);
    public DimensionPadding(int value) {
        this(value, value);
    }
    public boolean hasEqualTopAndBottom() {
        return top == bottom;
    }
}
