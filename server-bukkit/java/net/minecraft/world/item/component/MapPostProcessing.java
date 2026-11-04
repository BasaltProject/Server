// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.component;
import io.netty.buffer.ByteBuf;
import java.util.function.IntFunction;
import net.minecraft.network.codec.StreamCodec;
public enum MapPostProcessing {
    LOCK, SCALE;
    public static final IntFunction<MapPostProcessing> ID_MAP = id -> values()[Math.floorMod(id, values().length)];
    public static final StreamCodec<ByteBuf, MapPostProcessing> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.component.MapPostProcessing.class, "STREAM_CODEC");
    public int id() {
        return ordinal();
    }
}
