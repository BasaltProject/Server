// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.state.properties;
import net.minecraft.core.Direction;
import net.minecraft.util.StringRepresentable;
public enum DoubleBlockHalf implements StringRepresentable {
    UPPER, LOWER;
    public Direction getDirectionToOther() {
        return this == UPPER ? net.minecraft.core.Direction.DOWN : net.minecraft.core.Direction.UP;
    }
    public String toString() {
        return getSerializedName();
    }
    public String getSerializedName() {
        return name().toLowerCase(java.util.Locale.ROOT);
    }
    public DoubleBlockHalf getOtherHalf() {
        return this == UPPER ? LOWER : UPPER;
    }
}
