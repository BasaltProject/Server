// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.login;
import java.util.Arrays;
import net.minecraft.util.Crypt;
import java.security.PrivateKey;
import java.security.PublicKey;
import javax.crypto.SecretKey;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.util.CryptException;
public class ServerboundKeyPacket implements Packet<ServerLoginPacketListener> {
    private final byte[] keybytes;
    private final byte[] encryptedChallenge;
    public static final StreamCodec<FriendlyByteBuf, ServerboundKeyPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ServerboundKeyPacket", () -> Packet.codec(ServerboundKeyPacket::write, ServerboundKeyPacket::new));
    public ServerboundKeyPacket(SecretKey secretKey, PublicKey publicKey, byte[] challenge) throws CryptException {
        this.keybytes = Crypt.encryptUsingKey(publicKey, secretKey.getEncoded());
        this.encryptedChallenge = Crypt.encryptUsingKey(publicKey, challenge);
    }
    private ServerboundKeyPacket(FriendlyByteBuf input) {
        this.keybytes = input.readByteArray();
        this.encryptedChallenge = input.readByteArray();
    }
    private void write(FriendlyByteBuf output) {
        output.writeByteArray(this.keybytes);
        output.writeByteArray(this.encryptedChallenge);
    }
    public PacketType<ServerboundKeyPacket> type() {
        return LoginPacketTypes.SERVERBOUND_KEY;
    }
    public void handle(ServerLoginPacketListener listener) {
        listener.handleKey(this);
    }
    public SecretKey getSecretKey(PrivateKey privateKey) throws CryptException {
        return Crypt.decryptByteToSecretKey(privateKey, keybytes);
    }
    public boolean isChallengeValid(byte[] challenge, PrivateKey privateKey) {
        try {
            return java.util.Arrays.equals(challenge, Crypt.decryptUsingKey(privateKey, encryptedChallenge));
        } catch (CryptException e) {
            return false;
        }
    }
}
