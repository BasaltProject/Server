// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.login;
import java.util.UUID;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
public record ServerboundHelloPacket(String name, UUID profileId) implements Packet<ServerLoginPacketListener> {
    public static final StreamCodec<FriendlyByteBuf, ServerboundHelloPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ServerboundHelloPacket", () -> StreamCodec.of((output, packet) -> {
            output.writeUtf(packet.name(), 16);
            output.writeUUID(packet.profileId());
        }, input -> new ServerboundHelloPacket(input.readUtf(16), input.readUUID())));
    private ServerboundHelloPacket(FriendlyByteBuf input) {
        this(input.readUtf(16), input.readUUID());
    }
    public PacketType<ServerboundHelloPacket> type() {
        return LoginPacketTypes.SERVERBOUND_HELLO;
    }
    public void handle(ServerLoginPacketListener listener) {
        listener.handleHello(this);
    }
}
