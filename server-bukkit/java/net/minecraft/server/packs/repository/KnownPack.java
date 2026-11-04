// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.packs.repository;
import net.minecraft.network.codec.ByteBufCodecs;
import io.netty.buffer.ByteBuf;
import net.minecraft.network.codec.StreamCodec;
public record KnownPack(String namespace, String id, String version) {
    public static final StreamCodec<ByteBuf, KnownPack> STREAM_CODEC = StreamCodec.composite(ByteBufCodecs.STRING_UTF8, KnownPack::namespace, ByteBufCodecs.STRING_UTF8, KnownPack::id,
            ByteBufCodecs.STRING_UTF8, KnownPack::version, KnownPack::new);
    public static final String VANILLA_NAMESPACE = "minecraft";
    public static KnownPack vanilla(String id) {
        return new KnownPack(VANILLA_NAMESPACE, id, net.minecraft.SharedConstants.getCurrentVersion().id());
    }
    public boolean isVanilla() {
        return namespace.equals(VANILLA_NAMESPACE);
    }
}
