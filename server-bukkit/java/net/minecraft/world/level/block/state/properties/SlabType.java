// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.state.properties;
import net.minecraft.util.StringRepresentable;
public enum SlabType implements StringRepresentable {
    TOP, BOTTOM, DOUBLE;
    public String toString() {
        return getSerializedName();
    }
    public String getSerializedName() {
        return name().toLowerCase(java.util.Locale.ROOT);
    }
}
