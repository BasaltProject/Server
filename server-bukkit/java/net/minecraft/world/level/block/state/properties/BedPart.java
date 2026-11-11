// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.state.properties;
import com.mojang.serialization.Codec;
import net.minecraft.util.StringRepresentable;
public enum BedPart implements StringRepresentable {
    HEAD, FOOT;
    public static final Codec<BedPart> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.block.state.properties.BedPart.class, "CODEC");
    public String toString() {
        return getSerializedName();
    }
    public String getSerializedName() {
        return name().toLowerCase(java.util.Locale.ROOT);
    }
}
