// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.debug;

import io.netty.buffer.ByteBuf;
import java.util.List;
import java.util.Optional;
import net.minecraft.core.BlockPos;
import net.minecraft.network.codec.StreamCodec;

public record DebugBeeInfo(Optional<BlockPos> hivePos, Optional<BlockPos> flowerPos, int travelTicks, List<BlockPos> blacklistedHives) {
    public static final StreamCodec<ByteBuf, DebugBeeInfo> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(DebugBeeInfo.class, "STREAM_CODEC");

    public boolean hasHive(BlockPos hivePos) {
        return this.hivePos.isPresent() && hivePos.equals(this.hivePos.get());
    }
}
