// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.entity;

import io.netty.buffer.ByteBuf;
import net.minecraft.network.codec.StreamCodec;
public enum SignTextSlot {
    BACK, FRONT;
;
    public static final StreamCodec<ByteBuf, SignTextSlot> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.level.block.entity.SignTextSlot.class, "STREAM_CODEC");
}
