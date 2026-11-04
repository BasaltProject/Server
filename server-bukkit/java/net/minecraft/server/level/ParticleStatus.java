// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.level;

import com.mojang.serialization.Codec;
import io.netty.buffer.ByteBuf;
import net.minecraft.network.chat.Component;
import net.minecraft.network.codec.StreamCodec;
public enum ParticleStatus {
    ALL, DECREASED, MINIMAL;
;
    public static final Codec<ParticleStatus> LEGACY_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.server.level.ParticleStatus.class, "LEGACY_CODEC");
    public static final StreamCodec<ByteBuf, ParticleStatus> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.server.level.ParticleStatus.class, "STREAM_CODEC");
    public Component caption() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.server.level.ParticleStatus#caption");
    }
}
