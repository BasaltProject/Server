// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.login;
import net.minecraft.core.UUIDUtil;
import net.minecraft.network.codec.ByteBufCodecs;
import com.mojang.authlib.GameProfile;
import io.netty.buffer.ByteBuf;
import java.util.UUID;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
public record ClientboundLoginFinishedPacket(GameProfile gameProfile, UUID sessionId) implements Packet<ClientLoginPacketListener> {
    public static final StreamCodec<ByteBuf, ClientboundLoginFinishedPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ClientboundLoginFinishedPacket.STREAM_CODEC", () -> StreamCodec.composite(ByteBufCodecs.GAME_PROFILE, ClientboundLoginFinishedPacket::gameProfile, UUIDUtil.STREAM_CODEC, ClientboundLoginFinishedPacket::sessionId, ClientboundLoginFinishedPacket::new));
    public PacketType<ClientboundLoginFinishedPacket> type() {
        return LoginPacketTypes.CLIENTBOUND_LOGIN_FINISHED;
    }
    public void handle(ClientLoginPacketListener listener) {
        listener.handleLoginFinished(this);
    }
    public boolean isTerminal() {
        return true;
    }
}
