// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import java.util.function.IntFunction;
import net.minecraft.core.particles.ParticleTypes;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.util.ByIdMap;
import io.netty.buffer.ByteBuf;
import net.minecraft.core.particles.ParticleOptions;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
public record ClientboundLevelParticlesPacket(ParticleOptions particle, boolean overrideLimiter, boolean alwaysShow, double x, double y, double z, float xDist, float yDist, float zDist, float xMaxSpeed, float yMaxSpeed, float zMaxSpeed, int count, RandomizationType randomizationType) implements Packet<ClientGamePacketListener> {
    public static final StreamCodec<RegistryFriendlyByteBuf, ClientboundLevelParticlesPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ClientboundLevelParticlesPacket.STREAM_CODEC", () -> StreamCodec.composite(ParticleTypes.STREAM_CODEC, ClientboundLevelParticlesPacket::particle, ByteBufCodecs.BOOL, ClientboundLevelParticlesPacket::overrideLimiter, ByteBufCodecs.BOOL, ClientboundLevelParticlesPacket::alwaysShow, ByteBufCodecs.DOUBLE, ClientboundLevelParticlesPacket::x, ByteBufCodecs.DOUBLE, ClientboundLevelParticlesPacket::y, ByteBufCodecs.DOUBLE, ClientboundLevelParticlesPacket::z, ByteBufCodecs.FLOAT, ClientboundLevelParticlesPacket::xDist, ByteBufCodecs.FLOAT, ClientboundLevelParticlesPacket::yDist, ByteBufCodecs.FLOAT, ClientboundLevelParticlesPacket::zDist, ByteBufCodecs.FLOAT, ClientboundLevelParticlesPacket::xMaxSpeed, ByteBufCodecs.FLOAT, ClientboundLevelParticlesPacket::yMaxSpeed, ByteBufCodecs.FLOAT, ClientboundLevelParticlesPacket::zMaxSpeed, ByteBufCodecs.VAR_INT, ClientboundLevelParticlesPacket::count, ClientboundLevelParticlesPacket.RandomizationType.STREAM_CODEC, ClientboundLevelParticlesPacket::randomizationType, ClientboundLevelParticlesPacket::new));
    public ClientboundLevelParticlesPacket(ParticleOptions particle, boolean overrideLimiter, boolean alwaysShow, double x, double y, double z, float xDist, float yDist, float zDist, float maxSpeed, int count) {
        this(particle, overrideLimiter, alwaysShow, x, y, z, xDist, yDist, zDist, maxSpeed, maxSpeed, maxSpeed, count, RandomizationType.DEFAULT);
    }
    public PacketType<ClientboundLevelParticlesPacket> type() {
        return GamePacketTypes.CLIENTBOUND_LEVEL_PARTICLES;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handleParticleEvent(this);
    }
    public static enum RandomizationType {
        DEFAULT, ALTERNATIVE, ALTERNATIVE_WITH_SPEED;
        public static final StreamCodec<ByteBuf, RandomizationType> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.network.protocol.game.ClientboundLevelParticlesPacket.RandomizationType.class, "STREAM_CODEC");
        public boolean isAlternative() {
            return this != DEFAULT;
        }
    }
}
