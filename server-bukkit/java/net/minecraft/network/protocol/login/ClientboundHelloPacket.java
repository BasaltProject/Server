// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.login;
import net.minecraft.util.Crypt;
import java.security.PublicKey;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.util.CryptException;
public class ClientboundHelloPacket implements Packet<ClientLoginPacketListener> {
    private final String serverId;
    private final byte[] publicKey;
    private final byte[] challenge;
    private final boolean shouldAuthenticate;
    public static final StreamCodec<FriendlyByteBuf, ClientboundHelloPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ClientboundHelloPacket", () -> Packet.codec(ClientboundHelloPacket::write, ClientboundHelloPacket::new));
    public ClientboundHelloPacket(String serverId, byte[] publicKey, byte[] challenge, boolean shouldAuthenticate) {
        this.serverId = serverId;
        this.publicKey = publicKey;
        this.challenge = challenge;
        this.shouldAuthenticate = shouldAuthenticate;
    }
    private ClientboundHelloPacket(FriendlyByteBuf input) {
        this.serverId = input.readUtf(20);
        this.publicKey = input.readByteArray();
        this.challenge = input.readByteArray();
        this.shouldAuthenticate = input.readBoolean();
    }
    private void write(FriendlyByteBuf output) {
        output.writeUtf(this.serverId);
        output.writeByteArray(this.publicKey);
        output.writeByteArray(this.challenge);
        output.writeBoolean(this.shouldAuthenticate);
    }
    public PacketType<ClientboundHelloPacket> type() {
        return LoginPacketTypes.CLIENTBOUND_HELLO;
    }
    public void handle(ClientLoginPacketListener listener) {
        listener.handleHello(this);
    }
    public String getServerId() {
        return serverId;
    }
    public PublicKey getPublicKey() throws CryptException {
        return Crypt.byteToPublicKey(publicKey);
    }
    public byte[] getChallenge() {
        return challenge;
    }
    public boolean shouldAuthenticate() {
        return shouldAuthenticate;
    }
}
