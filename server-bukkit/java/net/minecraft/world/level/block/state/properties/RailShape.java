// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.state.properties;
import net.minecraft.util.StringRepresentable;
public enum RailShape implements StringRepresentable {
    NORTH_SOUTH, EAST_WEST, ASCENDING_EAST, ASCENDING_WEST, ASCENDING_NORTH, ASCENDING_SOUTH, SOUTH_EAST, SOUTH_WEST, NORTH_WEST, NORTH_EAST;
    public String getName() {
        return name().toLowerCase(java.util.Locale.ROOT);
    }
    public String toString() {
        return getName();
    }
    public boolean isSlope() {
        return this == ASCENDING_NORTH || this == ASCENDING_EAST || this == ASCENDING_SOUTH || this == ASCENDING_WEST;
    }
    public String getSerializedName() {
        return name().toLowerCase(java.util.Locale.ROOT);
    }
}
