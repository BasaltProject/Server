// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.state.properties;
import net.minecraft.util.StringRepresentable;
public enum PotentSulfurState implements StringRepresentable {
    DRY, WET, DORMANT, ERUPTING, CONTINUOUS;
    public String toString() {
        return getSerializedName();
    }
    public String getSerializedName() {
        return name().toLowerCase(java.util.Locale.ROOT);
    }
}
