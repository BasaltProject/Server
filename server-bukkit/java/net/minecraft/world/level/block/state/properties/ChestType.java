// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.state.properties;
import com.mojang.serialization.Codec;
import net.minecraft.util.StringRepresentable;
public enum ChestType implements StringRepresentable {
    SINGLE, LEFT, RIGHT;
    public static final Codec<ChestType> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.block.state.properties.ChestType.class, "CODEC");
    public String getSerializedName() {
        return name().toLowerCase(java.util.Locale.ROOT);
    }
    public ChestType getOpposite() {
        return switch (this) {
            case SINGLE -> SINGLE;
            case LEFT -> RIGHT;
            case RIGHT -> LEFT;
        };
    }
}
