// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;

import com.mojang.serialization.Codec;
import io.netty.buffer.ByteBuf;
import net.minecraft.network.codec.StreamCodec;
public enum Unit {
    INSTANCE;

    public static final Codec<Unit> CODEC = com.mojang.serialization.MapCodec.unit(Unit.INSTANCE).codec();
    public static final StreamCodec<ByteBuf, Unit> STREAM_CODEC = StreamCodec.unit(Unit.INSTANCE);
}
