// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.state.properties;
import com.mojang.serialization.Codec;
import net.minecraft.network.chat.Component;
import net.minecraft.util.StringRepresentable;
public enum StructureMode implements StringRepresentable {
    SAVE, LOAD, CORNER, DATA;
    public static final Codec<StructureMode> LEGACY_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.block.state.properties.StructureMode.class, "LEGACY_CODEC");
    public String getSerializedName() {
        return name().toLowerCase(java.util.Locale.ROOT);
    }
    public Component getDisplayName() {
        return Component.translatable("structure_block.mode_info." + getSerializedName());



    }
}
