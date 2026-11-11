// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.state.properties;
import net.minecraft.util.StringRepresentable;
public enum Tilt implements StringRepresentable {
    NONE, UNSTABLE, PARTIAL, FULL;
    public String getSerializedName() {
        return name().toLowerCase(java.util.Locale.ROOT);
    }
    public boolean causesVibration() {
        return this != UNSTABLE;
    }
}
