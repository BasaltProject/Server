// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.debug;

import io.netty.buffer.ByteBuf;
import net.minecraft.network.codec.StreamCodec;

public enum DebugEntityBlockIntersection {
    IN_BLOCK(1610678016), IN_FLUID(1610612991), IN_AIR(1613968179);

    public static final StreamCodec<ByteBuf, DebugEntityBlockIntersection> STREAM_CODEC =
        org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(DebugEntityBlockIntersection.class, "STREAM_CODEC");
    private final int color;

    DebugEntityBlockIntersection(int color) {
        this.color = color;
    }

    public int color() {
        return color;
    }
}
