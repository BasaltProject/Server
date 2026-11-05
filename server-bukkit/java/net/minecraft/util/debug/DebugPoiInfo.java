// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.debug;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.entity.ai.village.poi.PoiRecord;
import net.minecraft.world.entity.ai.village.poi.PoiType;
public record DebugPoiInfo(BlockPos pos, Holder<PoiType> poiType, int freeTicketCount) {
    public static final StreamCodec<RegistryFriendlyByteBuf, DebugPoiInfo> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.util.debug.DebugPoiInfo.class, "STREAM_CODEC");
    public DebugPoiInfo(PoiRecord record) {
        this(record.getPos(), record.getPoiType(), record.getFreeTickets());
    }
}
