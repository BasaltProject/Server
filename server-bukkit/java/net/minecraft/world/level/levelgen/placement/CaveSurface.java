// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.placement;
import com.mojang.serialization.Codec;
import net.minecraft.core.Direction;
import net.minecraft.util.StringRepresentable;
public enum CaveSurface implements StringRepresentable {
    CEILING, FLOOR;
    private Direction direction() {
        return this == CEILING ? Direction.UP : Direction.DOWN;
    }
    public static final Codec<CaveSurface> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.placement.CaveSurface.class, "CODEC");
    public Direction getDirection() {
        return direction();
    }
    public int getY() {
        return this == CEILING ? 1 : -1;
    }
    public String getSerializedName() {
        return name().toLowerCase(java.util.Locale.ROOT);
    }
}
