// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.saveddata.maps;
import com.mojang.serialization.Codec;
import net.minecraft.core.BlockPos;
public record MapFrame(BlockPos pos, int rotation, int entityId) {
    public static final Codec<MapFrame> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.saveddata.maps.MapFrame.class, "CODEC");
    public String getId() {
        return frameId(pos);
    }
    public static String frameId(BlockPos pos) {
        return "frame-" + pos.getX() + "," + pos.getY() + "," + pos.getZ();
    }
}
